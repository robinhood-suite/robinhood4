/* This file is part of the RobinHood Library
 * Copyright (C) 2020 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "mongo.h"

/*----------------------------------------------------------------------------*
 |                    bson_append_rbh_filter_projection()                     |
 *----------------------------------------------------------------------------*/

static bool
bson_append_statx_projection(bson_t *bson, const char *key, size_t key_length,
                             unsigned int mask)
{
    bson_t document;

    /* cf. comment in bson_append_rbh_filter_projection() */
    return bson_append_document_begin(bson, key, key_length, &document)
        && BSON_APPEND_BOOL(&document, MFF_STATX_BLKSIZE, true)
        && BSON_APPEND_BOOL(&document, MFF_STATX_ATTRIBUTES, true)
        && BSON_APPEND_BOOL(&document, MFF_STATX_DEV, true)
        && BSON_APPEND_BOOL(&document, MFF_STATX_RDEV, true)
        && (!(mask & STATX_TYPE)
         || BSON_APPEND_BOOL(&document, MFF_STATX_TYPE, true))
        && (!(mask & STATX_MODE)
         || BSON_APPEND_BOOL(&document, MFF_STATX_MODE, true))
        && (!(mask & STATX_NLINK)
         || BSON_APPEND_BOOL(&document, MFF_STATX_NLINK, true))
        && (!(mask & STATX_UID)
         || BSON_APPEND_BOOL(&document, MFF_STATX_UID, true))
        && (!(mask & STATX_GID)
         || BSON_APPEND_BOOL(&document, MFF_STATX_GID, true))
        && (!(mask & STATX_ATIME)
         || BSON_APPEND_BOOL(&document, MFF_STATX_ATIME, true))
        && (!(mask & STATX_MTIME)
         || BSON_APPEND_BOOL(&document, MFF_STATX_MTIME, true))
        && (!(mask & STATX_CTIME)
         || BSON_APPEND_BOOL(&document, MFF_STATX_CTIME, true))
        && (!(mask & STATX_INO)
         || BSON_APPEND_BOOL(&document, MFF_STATX_INO, true))
        && (!(mask & STATX_SIZE)
         || BSON_APPEND_BOOL(&document, MFF_STATX_SIZE, true))
        && (!(mask & STATX_BLOCKS)
         || BSON_APPEND_BOOL(&document, MFF_STATX_BLOCKS, true))
        && (!(mask & STATX_BTIME)
         || BSON_APPEND_BOOL(&document, MFF_STATX_BTIME, true))
        && bson_append_document_end(bson, &document);
}

#define BSON_APPEND_STATX_PROJECTION(bson, key, mask) \
    bson_append_statx_projection(bson, key, strlen(key), mask)

static bool
bson_append_xattrs_projection(bson_t *bson, const char *key, size_t key_length,
                              const struct rbh_value_map *xattrs)
{
    bson_t document;

    /* An empty map means "get everything" */
    if (xattrs->count == 0)
        return bson_append_bool(bson, key, key_length, true);

    if (!bson_append_document_begin(bson, key, key_length, &document))
        return false;

    for (size_t i = 0; i < xattrs->count; i++) {
        const char *xattr = xattrs->pairs[i].key;

        if (!BSON_APPEND_BOOL(&document, xattr, true))
            return false;
    }

    return bson_append_document_end(bson, &document);
}

#define BSON_APPEND_XATTRS_PROJECTION(bson, key, map) \
    bson_append_xattrs_projection(bson, key, strlen(key), map)

bool
bson_append_rbh_filter_projection(
        bson_t *bson, const char *key, size_t key_length,
        const struct rbh_filter_projection *projection
        )
{
    unsigned int fsentry_mask = projection->fsentry_mask;
    bson_t document;

    /* MongoDB does not support mixing include/exclude projections, so we can't
     * send:
     *     {field-0: true, field-1: false, field-2: false, field-3: true}
     * It has to be either:
     *     {field-0: true, field-3: true}
     * Or:
     *     {field-1: false, field-2: false}
     *
     * That is because {field: true} is interpreted as "nothing other than
     * `field'", whereas {field: false} is interpreted as "everything but
     * `field'", and those two interpretation don't mix well with one another
     *
     * The exclusion approach does not work for xattrs (we don't know of "all
     * the xattrs", so we can't filter out all but those we wish to keep).
     * Therefore, we have to take the inclusion approach.
     *
     * This may be safer anyway, as it naturally hides "fields from the future".
     */

    if (fsentry_mask == 0)
        /* Filter out everything
         *
         * XXX: This does not work with "fields from the future". It may be
         *      better to simply filter in the "_id" field...
         */
        return bson_append_document_begin(bson, key, key_length, &document)
            && BSON_APPEND_BOOL(&document, MFF_ID, false)
            && BSON_APPEND_BOOL(&document, MFF_NAMESPACE, false)
            && BSON_APPEND_BOOL(&document, MFF_STATX, false)
            && BSON_APPEND_BOOL(&document, MFF_SYMLINK, false)
            && BSON_APPEND_BOOL(&document, MFF_XATTRS, false)
            && bson_append_document_end(bson, &document);

    return bson_append_document_begin(bson, key, key_length, &document)
        && (!(fsentry_mask & RBH_FP_ID)
         || BSON_APPEND_BOOL(&document, MFF_ID, true))
        && (!(fsentry_mask & RBH_FP_PARENT_ID)
         || BSON_APPEND_BOOL(&document, MFF_NAMESPACE "." MFF_PARENT_ID, true))
        && (!(fsentry_mask & RBH_FP_NAME)
         || BSON_APPEND_BOOL(&document, MFF_NAMESPACE "." MFF_NAME, true))
        && (!(fsentry_mask & RBH_FP_STATX)
         || BSON_APPEND_STATX_PROJECTION(&document, MFF_STATX,
                                         projection->statx_mask))
        && (!(fsentry_mask & RBH_FP_SYMLINK)
         || BSON_APPEND_BOOL(&document, MFF_SYMLINK, true))
        && (!(fsentry_mask & RBH_FP_NAMESPACE_XATTRS)
         || BSON_APPEND_XATTRS_PROJECTION(&document,
                                          MFF_NAMESPACE "." MFF_XATTRS,
                                          &projection->xattrs.ns))
        && (!(fsentry_mask & RBH_FP_INODE_XATTRS)
         || BSON_APPEND_XATTRS_PROJECTION(&document, MFF_XATTRS,
                                          &projection->xattrs.inode))
        && bson_append_document_end(bson, &document);
}

/*----------------------------------------------------------------------------*
 |                       bson_append_rbh_filter_sorts()                       |
 *----------------------------------------------------------------------------*/

#define XATTR_ONSTACK_LENGTH 128

bool
bson_append_rbh_filter_sorts(bson_t *bson, const char *key, size_t key_length,
                             const struct rbh_filter_sort *items, size_t count)
{
    bson_t document;

    if (!bson_append_document_begin(bson, key, key_length, &document))
        return false;

    for (size_t i = 0; i < count; i++) {
        char onstack[XATTR_ONSTACK_LENGTH];
        char *buffer = onstack;
        bool success;

        key = field2str(&items[i].field, &buffer, sizeof(onstack));
        if (key == NULL)
            return false;

        success = BSON_APPEND_INT32(&document, key,
                                    items[i].ascending ? 1 : -1);
        if (buffer != onstack)
            free(buffer);
        if (!success)
            return false;
    }

    return bson_append_document_end(bson, &document);
}
