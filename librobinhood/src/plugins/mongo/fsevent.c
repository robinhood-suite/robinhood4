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

#include "robinhood/fsevent.h"

#include "mongo.h"

/*----------------------------------------------------------------------------*
 |                         bson_update_from_fsevent()                         |
 *----------------------------------------------------------------------------*/

static bson_t *
bson_from_upsert(const struct rbh_value_map *xattrs,
                 const struct rbh_statx *statxbuf, const char *symlink)
{
    bson_t *bson = bson_new();
    int save_errno = ENOBUFS;
    bson_t set, unset, inc;

    bson_init(&set);
    if (statxbuf) {
        if (!BSON_APPEND_STATX(&set, MFF_STATX, statxbuf))
            goto out_destroy_set;
    }

    if (symlink) {
        if (!BSON_APPEND_UTF8(&set, MFF_SYMLINK, symlink))
            goto out_destroy_set;
    }

    if (!bson_append_setxattrs(&set, MFF_XATTRS, xattrs)) {
        save_errno = errno;
        goto out_destroy_set;
    }

    /* Empty $set documents are not allowed */
    if (!bson_empty(&set)) {
        if (!BSON_APPEND_DOCUMENT(bson, "$set", &set))
            goto out_destroy_set;
    }

    bson_init(&unset);
    if (!bson_append_unsetxattrs(&unset, MFF_XATTRS, xattrs)) {
        save_errno = errno;
        goto out_destroy_unset;
    }

    /* Empty $unset documents are not allowed */
    if (!bson_empty(&unset)) {
        if (!BSON_APPEND_DOCUMENT(bson, "$unset", &unset))
            goto out_destroy_unset;
    }

    bson_init(&inc);
    if (!bson_append_incxattrs(&inc, MFF_XATTRS, xattrs)) {
        save_errno = errno;
        goto out_destroy_inc;
    }

    /* Empty $inc documents are not allowed */
    if (!bson_empty(&inc)) {
        if (!BSON_APPEND_DOCUMENT(bson, "$inc", &inc))
            goto out_destroy_inc;
    }

    bson_destroy(&inc);
    bson_destroy(&unset);
    bson_destroy(&set);
    return bson;

out_destroy_inc:
    bson_destroy(&inc);
out_destroy_unset:
    bson_destroy(&unset);
out_destroy_set:
    bson_destroy(&set);
    bson_destroy(bson);
    errno = save_errno;
    return NULL;
}

static bson_t *
bson_from_link(const struct rbh_value_map *xattrs,
               const struct rbh_id *parent_id, const char *name)
{
    bson_t *bson = bson_new();
    bson_t document;
    bson_t subdoc;

    if (BSON_APPEND_DOCUMENT_BEGIN(bson, "$push", &document)
     && BSON_APPEND_DOCUMENT_BEGIN(&document, MFF_NAMESPACE, &subdoc)
     && BSON_APPEND_RBH_ID(&subdoc, MFF_PARENT_ID, parent_id)
     && BSON_APPEND_UTF8(&subdoc, MFF_NAME, name)
     && BSON_APPEND_RBH_VALUE_MAP(&subdoc, MFF_XATTRS, xattrs)
     && bson_append_document_end(&document, &subdoc)
     && bson_append_document_end(bson, &document))
        return bson;

    bson_destroy(bson);
    errno = ENOBUFS;
    return NULL;
}

static bson_t *
bson_from_unlink(const struct rbh_id *parent_id, const char *name)
{
    bson_t *bson = bson_new();
    bson_t document;
    bson_t subdoc;

    if (BSON_APPEND_DOCUMENT_BEGIN(bson, "$pull", &document)
     && BSON_APPEND_DOCUMENT_BEGIN(&document, MFF_NAMESPACE, &subdoc)
     && BSON_APPEND_RBH_ID(&subdoc, MFF_PARENT_ID, parent_id)
     && BSON_APPEND_UTF8(&subdoc, MFF_NAME, name)
     && bson_append_document_end(&document, &subdoc)
     && bson_append_document_end(bson, &document))
        return bson;

    bson_destroy(bson);
    errno = ENOBUFS;
    return NULL;
}

static bson_t *
bson_from_xattrs(const char *prefix, const struct rbh_value_map *xattrs)
{
    bson_t *bson = bson_new();
    int save_errno = ENOBUFS;
    bson_t set, unset, inc;

    bson_init(&set);
    if (!bson_append_setxattrs(&set, prefix, xattrs)) {
        save_errno = errno;
        goto out_destroy_set;
    }

    bson_init(&unset);
    if (!bson_append_unsetxattrs(&unset, prefix, xattrs)) {
        save_errno = errno;
        goto out_destroy_unset;
    }

    bson_init(&inc);
    if (!bson_append_incxattrs(&inc, MFF_XATTRS, xattrs)) {
        save_errno = errno;
        goto out_destroy_inc;
    }

    /* Empty $set or $unset documents are not allowed */
    if (!bson_empty(&set)) {
        if (!BSON_APPEND_DOCUMENT(bson, "$set", &set))
            goto out_destroy_unset;
    }

    if (!bson_empty(&unset)) {
        if (!BSON_APPEND_DOCUMENT(bson, "$unset", &unset))
            goto out_destroy_unset;
    }

    /* Empty $inc documents are not allowed */
    if (!bson_empty(&inc)) {
        if (!BSON_APPEND_DOCUMENT(bson, "$inc", &inc))
            goto out_destroy_inc;
    }

    bson_destroy(&inc);
    bson_destroy(&unset);
    bson_destroy(&set);
    return bson;

out_destroy_inc:
    bson_destroy(&inc);
out_destroy_unset:
    bson_destroy(&unset);
out_destroy_set:
    bson_destroy(&set);
    bson_destroy(bson);
    errno = save_errno;
    return NULL;
}

static bson_t *
bson_from_ns_xattrs(const struct rbh_value_map *xattrs)
{
    return bson_from_xattrs(MFF_NAMESPACE ".$." MFF_XATTRS, xattrs);
}

static bson_t *
bson_from_inode_xattrs(const struct rbh_value_map *xattrs)
{
    return bson_from_xattrs(MFF_XATTRS, xattrs);
}

bson_t *
bson_update_from_fsevent(const struct rbh_fsevent *fsevent)
{
    switch (fsevent->type) {
    case RBH_FET_UPSERT:
        return bson_from_upsert(&fsevent->xattrs, fsevent->upsert.statx,
                                fsevent->upsert.symlink);
    case RBH_FET_LINK:
        return bson_from_link(&fsevent->xattrs, fsevent->link.parent_id,
                              fsevent->link.name);
    case RBH_FET_UNLINK:
        return bson_from_unlink(fsevent->link.parent_id, fsevent->link.name);
    case RBH_FET_XATTR:
        if (fsevent->ns.parent_id)
            return bson_from_ns_xattrs(&fsevent->xattrs);
        return bson_from_inode_xattrs(&fsevent->xattrs);
    default:
        errno = EINVAL;
        return NULL;
    }
}
