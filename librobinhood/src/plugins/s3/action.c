/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "robinhood/filter.h"
#include "robinhood/fsentry.h"
#include "robinhood/projection.h"
#include "robinhood/utils.h"

#include "s3_wrapper.h"

int
rbh_s3_delete_entry(struct rbh_fsentry *fsentry)
{
    char *bucket, *object;
    size_t bucket_len;
    char *path;
    char *sep;

    path = (char *) fsentry_path(fsentry);
    if (path == NULL)
        return -1;

    sep = strchr(path, '/');

    bucket_len = sep - path;
    path[bucket_len] = '\0';
    bucket = path;

    object = sep + 1;

    return s3_delete_object(bucket, object);
}

int
rbh_s3_fill_entry_info(char *output, int max_length,
                       const struct rbh_fsentry *fsentry,
                       const char *directive, const char *backend)
{
    const struct rbh_value *value;
    char buffer[1024];

    assert(directive != NULL);
    assert(*directive != '\0');

    switch(*directive) {
    case 'b':
        value = rbh_fsentry_find_inode_xattr(fsentry, "bucket");
        if (value == NULL)
            return 0;
        return snprintf(output, max_length, "%s", value->string);
    case 'f':
        return snprintf(output, max_length, "%s", fsentry->name);
    case 'H':
        return snprintf(output, max_length, "%s", backend);
    case 'I':
        if (base64_encode(buffer, fsentry->id.data, fsentry->id.size) == 0)
            return -1;

        return snprintf(output, max_length, "%s", buffer);
    case 'p':
        return snprintf(output, max_length, "%s", fsentry_path(fsentry));
    case 's':
        return snprintf(output, max_length, "%lu", fsentry->statx->stx_size);
    case 't':
        return snprintf(output, max_length, "%s",
                        time_from_timestamp(&fsentry->statx->stx_mtime.tv_sec));
    case 'T':
        return snprintf(output, max_length, "%lu",
                        fsentry->statx->stx_mtime.tv_sec);
    }

    return 0;
}

int
rbh_s3_fill_projection(struct rbh_filter_projection *projection,
                       const char *directive)
{
    assert(directive != NULL);
    assert(*directive != '\0');

    switch (*directive) {
    case 'b':
        rbh_projection_add(projection, str2filter_field("xattrs.bucket"));
        break;
    case 'f':
        rbh_projection_add(projection, str2filter_field("name"));
        break;
    case 'I':
        rbh_projection_add(projection, str2filter_field("id"));
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
        return 0;
    }

    return 1;
}
