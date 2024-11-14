/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <sys/stat.h>

#include "robinhood/fsentry.h"
#include "robinhood/statx.h"

#include "mongo.h"
#include "utils.h"

/*----------------------------------------------------------------------------*
 |                            fsentry_from_bson()                             |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                         bson_iter_rbh_id()                         |
     *--------------------------------------------------------------------*/

static void
_bson_iter_binary(bson_iter_t *iter, bson_subtype_t *subtype,
                  const char **data, size_t *size)
{
    uint32_t binary_len;

    if (BSON_ITER_HOLDS_NULL(iter)) {
        *subtype = BSON_SUBTYPE_BINARY;
        *size = 0;
        return;
    }

    bson_iter_binary(iter, subtype, &binary_len, (const uint8_t **)data);

    static_assert(SIZE_MAX >= UINT32_MAX, "");
    *size = binary_len;
}

bool
bson_iter_rbh_id(bson_iter_t *iter, struct rbh_id *id)
{
    bson_subtype_t subtype;

    _bson_iter_binary(iter, &subtype, &id->data, &id->size);
    if (subtype != BSON_SUBTYPE_BINARY) {
        errno = EINVAL;
        return false;
    }
    return true;
}

enum fsentry_token {
    FT_UNKNOWN,
    FT_ID,
    FT_NAMESPACE,
    FT_SYMLINK,
    FT_XATTRS,
    FT_STATX,
    FT_FORM,
};

static enum fsentry_token
fsentry_tokenizer(const char *key)
{
    switch (*key++) {
    case '_': /* _id */
        if (strcmp(key, "id"))
            break;
        return FT_ID;
    case 'f': /* form */
        if (strcmp(key, "orm"))
            break;
        return FT_FORM;
    case 'n': /* ns */
        if (strcmp(key, "s"))
            break;
        return FT_NAMESPACE;
    case 's': /* statx, symlink */
        switch (*key++) {
        case 't': /* statx */
            if (strcmp(key, "atx"))
                break;
            return FT_STATX;
        case 'y': /* symlink */
            if (strcmp(key, "mlink"))
                break;
            return FT_SYMLINK;
        }
        break;
    case 'x': /* xattrs */
        if (strcmp(key, "attrs"))
            break;
        return FT_XATTRS;
    }
    return FT_UNKNOWN;
}

static bool
bson_iter_fsentry(bson_iter_t *iter, struct rbh_fsentry *fsentry,
                  struct rbh_statx *statxbuf, const char **symlink,
                  char **buffer, size_t *bufsize)
{
    size_t size = *bufsize;
    char *data = *buffer;

    fsentry->mask = 0;
    *symlink = NULL;

    while (bson_iter_next(iter)) {
        bson_iter_t subiter;
        bson_iter_t tmp;

        switch (fsentry_tokenizer(bson_iter_key(iter))) {
        case FT_UNKNOWN:
            break;
        case FT_ID:
            if (!BSON_ITER_HOLDS_BINARY(iter))
                goto out_einval;

            if (!bson_iter_rbh_id(iter, &fsentry->id))
                return false;
            fsentry->mask |= RBH_FP_ID;
            break;
        case FT_NAMESPACE:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);

            if (!bson_iter_namespace(&subiter, fsentry, &data, &size))
                return false;
            break;
        case FT_SYMLINK:
            if (!BSON_ITER_HOLDS_UTF8(iter))
                goto out_einval;

            *symlink = bson_iter_utf8(iter, NULL);
            break;
        case FT_XATTRS:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);

            bson_iter_recurse(iter, &tmp);
            if (!bson_iter_rbh_value_map(&subiter, &fsentry->xattrs.inode,
                                         bson_iter_count(&tmp), &data, &size))
                return false;
            fsentry->mask |= RBH_FP_INODE_XATTRS;
            break;
        case FT_STATX:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);

            if (!bson_iter_statx(&subiter, statxbuf))
                return false;
            fsentry->statx = statxbuf;
            fsentry->mask |= RBH_FP_STATX;
            break;
        case FT_FORM:
            break;
        }
    }

    *bufsize = size;
    *buffer = data;
    return true;

out_einval:
    errno = EINVAL;
    return false;
}

static struct rbh_fsentry *
fsentry_almost_clone(const struct rbh_fsentry *fsentry, const char *symlink)
{
    struct {
        bool id:1;
        bool parent:1;
        bool name:1;
        bool statx:1;
        bool namespace_xattrs:1;
        bool inode_xattrs:1;
    } has = {
        .id = fsentry->mask & RBH_FP_ID,
        .parent = fsentry->mask & RBH_FP_PARENT_ID,
        .name = fsentry->mask & RBH_FP_NAME,
        .statx = fsentry->mask & RBH_FP_STATX,
        .namespace_xattrs = fsentry->mask & RBH_FP_NAMESPACE_XATTRS,
        .inode_xattrs = fsentry->mask & RBH_FP_INODE_XATTRS,
    };

    return rbh_fsentry_new(has.id ? &fsentry->id : NULL,
                           has.parent ? &fsentry->parent_id : NULL,
                           has.name ? fsentry->name : NULL,
                           has.statx ? fsentry->statx : NULL,
                           has.namespace_xattrs ? &fsentry->xattrs.ns : NULL,
                           has.inode_xattrs ? &fsentry->xattrs.inode : NULL,
                           symlink);
}

struct rbh_fsentry *
fsentry_from_bson(bson_iter_t *iter)
{
    struct rbh_fsentry fsentry;
    struct rbh_statx statxbuf;
    const char *symlink;
    char tmp[4096]; /* TODO: figure out a better size than the arbitrary 4096 */
    size_t bufsize = sizeof(tmp);
    char *buffer = tmp;

    if (!bson_iter_fsentry(iter, &fsentry, &statxbuf, &symlink, &buffer,
                           &bufsize))
        /* FIXME: while it is nice to try to parse most fsentries without
         *        allocating any memory using the "on stack" buffer `tmp', there
         *        should be a fallback for when that fails.
         */
        return NULL;

    return fsentry_almost_clone(&fsentry, symlink);
}
