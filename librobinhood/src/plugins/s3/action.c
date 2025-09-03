/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <string.h>

#include <robinhood/fsentry.h>

#include "s3_wrapper.h"

int
rbh_s3_delete_entry(struct rbh_fsentry *fsentry)
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

