/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <errno.h>
#include <fnmatch.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sysexits.h>

#include "robinhood/filters/core.h"
#include "robinhood/policyengine.h"
#include <robinhood.h>

struct rbh_mut_iterator *
rbh_collect_fsentry(const char *uri, struct rbh_filter *filter)
{
    struct rbh_backend *backend = rbh_backend_from_uri(uri, true);
    if (!backend)
        error(EXIT_FAILURE, errno, "rbh_backend_from_uri failed");

    struct rbh_filter_options options = {
        .skip = 0,
        .limit = 0,
        .skip_error = true,
        .one = false,
        .verbose = false,
        .dry_run = false
    };

    struct rbh_filter_output output = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = RBH_FP_ID | RBH_FP_NAMESPACE_XATTRS,
            .statx_mask = 0,
        }
    };

    struct rbh_mut_iterator *it = rbh_backend_filter(backend, filter, &options,
                                                     &output, NULL);
    if (!it)
        error(EXIT_FAILURE, errno, "rbh_backend_filter failed");

    return it;
}

static const char *
rbh_pe_get_path(const struct rbh_fsentry *fsentry)
{
    const struct rbh_value *path_value;

    path_value = rbh_fsentry_find_ns_xattr(fsentry, "path");
    if (path_value == NULL || path_value->type != RBH_VT_STRING)
        return NULL;

    return path_value->string;
}

static const struct rbh_value *
get_field_value(const struct rbh_fsentry *fsentry,
                const struct rbh_filter_field *field)
{
    static struct rbh_value value;

    switch (field->fsentry) {
    case RBH_FP_NAME:
        if (!(fsentry->mask & RBH_FP_NAME))
            return NULL;
        value.type = RBH_VT_STRING;
        value.string = fsentry->name;
        return &value;

    case RBH_FP_SYMLINK:
        if (!(fsentry->mask & RBH_FP_SYMLINK))
            return NULL;
        value.type = RBH_VT_STRING;
        value.string = fsentry->symlink;
        return &value;

    case RBH_FP_NAMESPACE_XATTRS:
        if (!(fsentry->mask & RBH_FP_NAMESPACE_XATTRS))
            return NULL;
        if (field->xattr == NULL)
            return NULL;
        return rbh_fsentry_find_ns_xattr(fsentry, field->xattr);

    case RBH_FP_INODE_XATTRS:
        if (!(fsentry->mask & RBH_FP_INODE_XATTRS))
            return NULL;
        if (field->xattr == NULL)
            return NULL;
        return rbh_fsentry_find_inode_xattr(fsentry, field->xattr);

    case RBH_FP_STATX:
        if (!(fsentry->mask & RBH_FP_STATX))
            return NULL;

        const struct rbh_statx *statx = fsentry->statx;

        switch (field->statx) {
        case RBH_STATX_TYPE:
            if (!(statx->stx_mask & RBH_STATX_TYPE))
                return NULL;
            value.type = RBH_VT_INT32;
            value.int32 = statx->stx_mode & S_IFMT;
            return &value;

        case RBH_STATX_MODE:
            if (!(statx->stx_mask & RBH_STATX_MODE))
                return NULL;
            value.type = RBH_VT_UINT32;
            value.uint32 = statx->stx_mode;
            return &value;

        case RBH_STATX_SIZE:
            if (!(statx->stx_mask & RBH_STATX_SIZE))
                return NULL;
            value.type = RBH_VT_UINT64;
            value.uint64 = statx->stx_size;
            return &value;

        case RBH_STATX_ATIME_SEC:
            if (!(statx->stx_mask & RBH_STATX_ATIME_SEC))
                return NULL;
            value.type = RBH_VT_UINT64;
            value.uint64 = statx->stx_atime.tv_sec;
            return &value;

        case RBH_STATX_MTIME_SEC:
            if (!(statx->stx_mask & RBH_STATX_MTIME_SEC))
                return NULL;
            value.type = RBH_VT_UINT64;
            value.uint64 = statx->stx_mtime.tv_sec;
            return &value;

        case RBH_STATX_CTIME_SEC:
            if (!(statx->stx_mask & RBH_STATX_CTIME_SEC))
                return NULL;
            value.type = RBH_VT_UINT64;
            value.uint64 = statx->stx_ctime.tv_sec;
            return &value;

        case RBH_STATX_BTIME_SEC:
            if (!(statx->stx_mask & RBH_STATX_BTIME_SEC))
                return NULL;
            value.type = RBH_VT_UINT64;
            value.uint64 = statx->stx_btime.tv_sec;
            return &value;

        case RBH_STATX_UID:
            if (!(statx->stx_mask & RBH_STATX_UID))
                return NULL;
            value.type = RBH_VT_UINT64;
            value.uint64 = statx->stx_uid;
            return &value;

        case RBH_STATX_GID:
            if (!(statx->stx_mask & RBH_STATX_GID))
                return NULL;
            value.type = RBH_VT_UINT64;
            value.uint64 = statx->stx_gid;
            return &value;

        case RBH_STATX_NLINK:
            if (!(statx->stx_mask & RBH_STATX_NLINK))
                return NULL;
            value.type = RBH_VT_UINT64;
            value.uint64 = statx->stx_nlink;
            return &value;

        case RBH_STATX_BLOCKS:
            if (!(statx->stx_mask & RBH_STATX_BLOCKS))
                return NULL;
            value.type = RBH_VT_UINT64;
            value.uint64 = statx->stx_blocks;
            return &value;

        case RBH_STATX_INO:
            if (!(statx->stx_mask & RBH_STATX_INO))
                return NULL;
            value.type = RBH_VT_UINT64;
            value.uint64 = statx->stx_ino;
            return &value;

        default:
            return NULL;
        }

    default:
        return NULL;
    }
}
