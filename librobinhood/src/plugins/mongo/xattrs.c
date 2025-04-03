/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>

#include "robinhood/value.h"
#include "robinhood/utils.h"

#include "mongo.h"
#include "utils.h"

/*----------------------------------------------------------------------------*
 |                          bson_append_setxattrs()                           |
 *----------------------------------------------------------------------------*/

#define ONSTACK_KEYLEN_MAX 128

static bool
bson_append_xattr(bson_t *bson, const char *prefix, const char *xattr,
                  const struct rbh_value *value)
{
    char onstack[ONSTACK_KEYLEN_MAX];
    char *key = onstack;
    bool success;
    int keylen;

    keylen = snprintf(key, sizeof(onstack), "%s.%s", prefix, xattr);
    if (keylen < 0)
        return false;
    if ((size_t)keylen >= sizeof(onstack))
        keylen = asprintf(&key, "%s.%s", prefix, xattr);
    if (keylen < 0)
        return false;

    if (value == NULL)
        success = bson_append_null(bson, key, keylen);
    else
        success = bson_append_rbh_value(bson, key, keylen, value);
    if (key != onstack)
        free(key);

    errno = ENOBUFS;
    return success;
}

bool
bson_append_setxattrs(bson_t *bson, const char *prefix,
                      const struct rbh_value_map *xattrs)
{
    for (size_t i = 0; i < xattrs->count; i++) {
        const struct rbh_value *value = xattrs->pairs[i].value;
        const char *xattr = xattrs->pairs[i].key;

        /* Skip xattrs that are to be unset */
        if (value == NULL || strcmp(xattr, "nb_children") == 0)
            continue;

        if (!bson_append_xattr(bson, prefix, xattr, value))
            return false;
    }

    return true;
}

/*----------------------------------------------------------------------------*
 |                         bson_append_unsetxattrs()                          |
 *----------------------------------------------------------------------------*/

bool
bson_append_unsetxattrs(bson_t *bson, const char *prefix,
                        const struct rbh_value_map *xattrs)
{
    for (size_t i = 0; i < xattrs->count; i++) {
        const struct rbh_value *value = xattrs->pairs[i].value;
        const char *xattr = xattrs->pairs[i].key;

        /* Skip xattrs that are to be set */
        if (value || strcmp(xattr, "nb_children") == 0)
            continue;

        if (!bson_append_xattr(bson, prefix, xattr, NULL))
            return false;
    }

    return true;
}

/*----------------------------------------------------------------------------*
 |                         bson_append_incxattrs()                            |
 *----------------------------------------------------------------------------*/

bool
bson_append_incxattrs(bson_t *bson, const char *prefix,
                      const struct rbh_value_map *xattrs)
{
    for (size_t i = 0; i < xattrs->count; i++) {
        const struct rbh_value *value = xattrs->pairs[i].value;
        const char *xattr = xattrs->pairs[i].key;

        /* Skip xattrs that are to be set */
        if (strcmp(xattr, "nb_children"))
            continue;

        if (!bson_append_xattr(bson, prefix, xattr, value))
            return false;
        break;
    }

    return true;
}

enum namespace_token {
    NT_UNKNOWN,
    NT_PARENT,
    NT_NAME,
    NT_XATTRS,
};

static enum namespace_token
namespace_tokenizer(const char *key)
{
    switch (*key++) {
    case 'n': /* name */
        if (strcmp(key, "ame"))
            break;
        return NT_NAME;
    case 'p': /* parent */
        if (strcmp(key, "arent"))
            break;
        return NT_PARENT;
    case 'x': /* xattrs */
        if (strcmp(key, "attrs"))
            break;
        return NT_XATTRS;
    }
    return NT_UNKNOWN;
}

bool
bson_iter_namespace(bson_iter_t *iter, struct rbh_fsentry *fsentry,
                    char **buffer, size_t *bufsize)
{
    size_t size = *bufsize;
    char *data = *buffer;

    while (bson_iter_next(iter)) {
        bson_iter_t subiter;
        bson_iter_t tmp;

        switch (namespace_tokenizer(bson_iter_key(iter))) {
        case NT_UNKNOWN:
            break;
        case NT_PARENT:
            if (!BSON_ITER_HOLDS_NULL(iter) && !BSON_ITER_HOLDS_BINARY(iter))
                goto out_einval;

            if (!bson_iter_rbh_id(iter, &fsentry->parent_id))
                return false;
            fsentry->mask |= RBH_FP_PARENT_ID;
            break;
        case NT_NAME:
            if (!BSON_ITER_HOLDS_UTF8(iter))
                goto out_einval;

            fsentry->name = bson_iter_utf8(iter, NULL);
            fsentry->mask |= RBH_FP_NAME;
            break;
        case NT_XATTRS:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);

            bson_iter_recurse(iter, &tmp);
            if (!bson_iter_rbh_value_map(&subiter, &fsentry->xattrs.ns,
                                         bson_iter_count(&tmp), &data, &size))
                return false;
            fsentry->mask |= RBH_FP_NAMESPACE_XATTRS;
            break;
        }
    }

    *buffer = data;
    *bufsize = size;
    return true;

out_einval:
    errno = EINVAL;
    return false;
}

