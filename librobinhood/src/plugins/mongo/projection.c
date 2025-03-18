/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/statx.h"

#include "mongo.h"

/*----------------------------------------------------------------------------*
 |                    bson_append_rbh_filter_projection()                     |
 *----------------------------------------------------------------------------*/

static bool
bson_append_statx_projection(bson_t *bson, const char *key, size_t key_length,
                             unsigned int mask)
{
    bson_t document;
    bson_t subdoc;

    /* cf. comment in bson_append_rbh_filter_projection() */
    return bson_append_document_begin(bson, key, key_length, &document)
        && (!(mask & RBH_STATX_TYPE)
         || BSON_APPEND_BOOL(&document, MFF_STATX_TYPE, true))
        && (!(mask & RBH_STATX_MODE)
         || BSON_APPEND_BOOL(&document, MFF_STATX_MODE, true))
        && (!(mask & RBH_STATX_NLINK)
         || BSON_APPEND_BOOL(&document, MFF_STATX_NLINK, true))
        && (!(mask & RBH_STATX_UID)
         || BSON_APPEND_BOOL(&document, MFF_STATX_UID, true))
        && (!(mask & RBH_STATX_GID)
         || BSON_APPEND_BOOL(&document, MFF_STATX_GID, true))
        && (!(mask & RBH_STATX_ATIME)
         || (BSON_APPEND_DOCUMENT_BEGIN(&document, MFF_STATX_ATIME, &subdoc)
             && (!(mask & RBH_STATX_ATIME_SEC)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_TIMESTAMP_SEC, true))
             && (!(mask & RBH_STATX_ATIME_NSEC)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_TIMESTAMP_NSEC, true))
             && bson_append_document_end(&document, &subdoc)))
        && (!(mask & RBH_STATX_BTIME)
         || (BSON_APPEND_DOCUMENT_BEGIN(&document, MFF_STATX_BTIME, &subdoc)
             && (!(mask & RBH_STATX_BTIME_SEC)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_TIMESTAMP_SEC, true))
             && (!(mask & RBH_STATX_BTIME_NSEC)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_TIMESTAMP_NSEC, true))
             && bson_append_document_end(&document, &subdoc)))
        && (!(mask & RBH_STATX_CTIME)
         || (BSON_APPEND_DOCUMENT_BEGIN(&document, MFF_STATX_CTIME, &subdoc)
             && (!(mask & RBH_STATX_CTIME_SEC)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_TIMESTAMP_SEC, true))
             && (!(mask & RBH_STATX_CTIME_NSEC)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_TIMESTAMP_NSEC, true))
             && bson_append_document_end(&document, &subdoc)))
        && (!(mask & RBH_STATX_MTIME)
         || (BSON_APPEND_DOCUMENT_BEGIN(&document, MFF_STATX_MTIME, &subdoc)
             && (!(mask & RBH_STATX_MTIME_SEC)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_TIMESTAMP_SEC, true))
             && (!(mask & RBH_STATX_MTIME_NSEC)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_TIMESTAMP_NSEC, true))
             && bson_append_document_end(&document, &subdoc)))
        && (!(mask & RBH_STATX_INO)
         || BSON_APPEND_BOOL(&document, MFF_STATX_INO, true))
        && (!(mask & RBH_STATX_SIZE)
         || BSON_APPEND_BOOL(&document, MFF_STATX_SIZE, true))
        && (!(mask & RBH_STATX_BLOCKS)
         || BSON_APPEND_BOOL(&document, MFF_STATX_BLOCKS, true))
        && (!(mask & RBH_STATX_MNT_ID)
         || BSON_APPEND_BOOL(&document, MFF_STATX_MNT_ID, true))
        && (!(mask & RBH_STATX_BLKSIZE)
         || BSON_APPEND_BOOL(&document, MFF_STATX_BLKSIZE, true))
        && (!(mask & RBH_STATX_ATTRIBUTES)
         || BSON_APPEND_BOOL(&document, MFF_STATX_ATTRIBUTES, true))
        && (!(mask & RBH_STATX_DEV)
         || (BSON_APPEND_DOCUMENT_BEGIN(&document, MFF_STATX_DEV, &subdoc)
             && (!(mask & RBH_STATX_DEV_MAJOR)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_DEVICE_MAJOR, true))
             && (!(mask & RBH_STATX_DEV_MINOR)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_DEVICE_MINOR, true))
             && bson_append_document_end(&document, &subdoc)))
        && (!(mask & RBH_STATX_RDEV)
         || (BSON_APPEND_DOCUMENT_BEGIN(&document, MFF_STATX_RDEV, &subdoc)
             && (!(mask & RBH_STATX_RDEV_MAJOR)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_DEVICE_MAJOR, true))
             && (!(mask & RBH_STATX_RDEV_MINOR)
              || BSON_APPEND_BOOL(&subdoc, MFF_STATX_DEVICE_MINOR, true))
             && bson_append_document_end(&document, &subdoc)))
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

static bool
bson_append_fot_projection(bson_t *bson, const char *key, size_t key_length,
                           const struct rbh_filter_projection *projection)
{
    unsigned int fsentry_mask = projection->fsentry_mask;
    bson_t document;
    bson_t subdoc;

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
            && BSON_APPEND_UTF8(&document, "form", "fsentry")
            && BSON_APPEND_BOOL(&document, MFF_ID, false)
            && BSON_APPEND_BOOL(&document, MFF_NAMESPACE, false)
            && BSON_APPEND_BOOL(&document, MFF_STATX, false)
            && BSON_APPEND_BOOL(&document, MFF_SYMLINK, false)
            && BSON_APPEND_BOOL(&document, MFF_XATTRS, false)
            && bson_append_document_end(bson, &document);

    return bson_append_document_begin(bson, key, key_length, &document)
        && BSON_APPEND_UTF8(&document, "form", "fsentry")
        && (!(fsentry_mask & RBH_FP_ID)
         || BSON_APPEND_BOOL(&document, MFF_ID, true))
        && (!(fsentry_mask & RBH_FP_PARENT_ID ||
              fsentry_mask & RBH_FP_NAME ||
              fsentry_mask & RBH_FP_NAMESPACE_XATTRS)
         || (BSON_APPEND_DOCUMENT_BEGIN(&document, MFF_NAMESPACE, &subdoc)
             && (!(fsentry_mask & RBH_FP_PARENT_ID)
              || BSON_APPEND_BOOL(&subdoc, MFF_PARENT_ID, true))
             && (!(fsentry_mask & RBH_FP_NAME)
              || BSON_APPEND_BOOL(&subdoc, MFF_NAME, true))
             && (!(fsentry_mask & RBH_FP_NAMESPACE_XATTRS)
              || BSON_APPEND_XATTRS_PROJECTION(&subdoc, MFF_XATTRS,
                                               &projection->xattrs.ns))
             && bson_append_document_end(&document, &subdoc)))
        && (!(fsentry_mask & RBH_FP_STATX)
         || BSON_APPEND_STATX_PROJECTION(&document, MFF_STATX,
                                         projection->statx_mask))
        && (!(fsentry_mask & RBH_FP_SYMLINK)
         || BSON_APPEND_BOOL(&document, MFF_SYMLINK, true))
        && (!(fsentry_mask & RBH_FP_INODE_XATTRS)
         || BSON_APPEND_XATTRS_PROJECTION(&document, MFF_XATTRS,
                                          &projection->xattrs.inode))
        && bson_append_document_end(bson, &document);
}

/* The resulting bson will be as such:
 * { $project: { _id: 0, form: 'map', map: {
 *      'result': <accumulator>_<field>, ...}}}
 */
static bool
bson_append_fot_values(bson_t *bson, const char *key,
                       size_t key_length,
                       const struct rbh_group_fields *group,
                       const struct rbh_filter_output *output)
{
    bson_t document;
    bson_t subdoc;

    if (!(bson_append_document_begin(bson, key, key_length, &document)
          && ((group && group->id_fields == 0) ?
                BSON_APPEND_INT64(&document, "_id", 0) : true)
          && BSON_APPEND_UTF8(&document, "form", "map")
          && BSON_APPEND_DOCUMENT_BEGIN(&document, "map", &subdoc)))
        return false;

    for (size_t i = 0; i < output->output_fields.count; i++) {
        struct rbh_accumulator_field *field = &output->output_fields.fields[i];
        char accumulator[256];
        char bson_value[512];
        char field_str[256];
        char result[524];

        if (!get_accumulator_field_strings(field, accumulator, field_str,
                                           bson_value))
            return false;

        if (sprintf(result, "result_%s", bson_value) <= 0)
            return false;

        if (!BSON_APPEND_UTF8(&subdoc, result, bson_value))
            return false;
    }

    return bson_append_document_end(&document, &subdoc)
        && bson_append_document_end(bson, &document);
}

bool
bson_append_aggregate_projection_stage(bson_t *bson, const char *key,
                                       size_t key_length,
                                       const struct rbh_group_fields *group,
                                       const struct rbh_filter_output *output)
{
    if (output->type == RBH_FOT_PROJECTION)
        return bson_append_fot_projection(bson, key, key_length,
                                          &output->projection);
    else
        return bson_append_fot_values(bson, key, key_length, group, output);
}
