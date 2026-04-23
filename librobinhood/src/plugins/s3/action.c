/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "robinhood/action.h"
#include "robinhood/backend.h"
#include "robinhood/filter.h"
#include "robinhood/fsentry.h"
#include "robinhood/projection.h"
#include "robinhood/utils.h"

#include "s3_wrapper.h"
#include "s3_internals.h"

#define RBH_S3_DIRECTIVE_CATEGORY_CHARACTER '3'

int
rbh_s3_delete_entry(struct rbh_backend *backend, struct rbh_fsentry *fsentry,
                    const struct rbh_delete_params *params)
{
    const struct rbh_value *value;
    char *bucket, *object;
    size_t bucket_len;
    char *path;
    char *sep;

    value = rbh_fsentry_find_ns_xattr(fsentry, "path");
    if (value == NULL)
        return -1;

    path = (char *) value->string;
    sep = strchr(path, '/');

    bucket_len = sep - path;
    path[bucket_len] = '\0';
    bucket = path;

    object = sep + 1;

    return s3_delete_object(bucket, object);
}

enum known_directive
rbh_s3_fill_entry_info(const struct rbh_fsentry *fsentry,
                       const char *format_string, size_t *index,
                       char *output, size_t *output_length, int max_length,
                       const char *backend)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;
    const struct rbh_value *value;
    int tmp_length = 0;

    if (format_string[*index + 1] != RBH_S3_DIRECTIVE_CATEGORY_CHARACTER)
        return RBH_DIRECTIVE_UNKNOWN;

    switch (format_string[*index + 2]) {
    case 'b':
        value = rbh_fsentry_find_inode_xattr(fsentry, "bucket");
        if (value == NULL)
            tmp_length = snprintf(output, max_length, "None");
        else
            tmp_length = snprintf(output, max_length, "%s", value->string);

        break;
    case 'p':
        tmp_length = snprintf(output, max_length,
                              "%s", rbh_fsentry_find_ns_xattr(fsentry,
                                                              "path")->string);
        break;
    case 's':
        tmp_length = snprintf(output, max_length,
                              "%lu", fsentry->statx->stx_size);
        break;
    case 't':
        tmp_length = snprintf(
            output, max_length,
            "%s", time_from_timestamp(&fsentry->statx->stx_mtime.tv_sec)
        );
        break;
    case 'T':
        tmp_length = snprintf(output, max_length,
                              "%lu", fsentry->statx->stx_mtime.tv_sec);
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (tmp_length < 0)
        return RBH_DIRECTIVE_ERROR;

    if (rc == RBH_DIRECTIVE_KNOWN) {
        *output_length += tmp_length;
        *index += 2;
    }

    return rc;
}

enum known_directive
rbh_s3_fill_projection(struct rbh_filter_projection *projection,
                       const char *format_string, size_t *index)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;

    if (format_string[*index + 1] != RBH_S3_DIRECTIVE_CATEGORY_CHARACTER)
        return RBH_DIRECTIVE_UNKNOWN;

    switch (format_string[*index + 2]) {
    case 'b':
        rbh_projection_add(projection, str2filter_field("xattrs.bucket"));
        break;
    case 'p':
        rbh_projection_add(projection, str2filter_field("ns-xattrs"));
        break;
    case 's':
        rbh_projection_add(projection, str2filter_field("statx.size"));
        break;
    case 't':
    case 'T':
        rbh_projection_add(projection, str2filter_field("statx.mtime.sec"));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        *index += 2;

    return rc;
}
