/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
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
#include <robinhood.h>

bool
compare_values(enum rbh_filter_operator op,
               const struct rbh_value *field_val,
               const struct rbh_value *filter_val)
{
    if (field_val->type != filter_val->type)
        return false;

    switch (op) {
    case RBH_FOP_EQUAL:
        switch (field_val->type) {
        case RBH_VT_INT32:
            return field_val->int32 == filter_val->int32;
        case RBH_VT_UINT32:
            return field_val->uint32 == filter_val->uint32;
        case RBH_VT_INT64:
            return field_val->int64 == filter_val->int64;
        case RBH_VT_UINT64:
            return field_val->uint64 == filter_val->uint64;
        case RBH_VT_STRING:
            return strcmp(field_val->string, filter_val->string) == 0;
        default:
            return false;
        }

    case RBH_FOP_STRICTLY_LOWER:
        switch (field_val->type) {
        case RBH_VT_INT32:
            return field_val->int32 < filter_val->int32;
        case RBH_VT_UINT32:
            return field_val->uint32 < filter_val->uint32;
        case RBH_VT_INT64:
            return field_val->int64 < filter_val->int64;
        case RBH_VT_UINT64:
            return field_val->uint64 < filter_val->uint64;
        default:
            return false;
        }

    case RBH_FOP_STRICTLY_GREATER:
        switch (field_val->type) {
        case RBH_VT_INT32:
            return field_val->int32 > filter_val->int32;
        case RBH_VT_UINT32:
            return field_val->uint32 > filter_val->uint32;
        case RBH_VT_INT64:
            return field_val->int64 > filter_val->int64;
        case RBH_VT_UINT64:
            return field_val->uint64 > filter_val->uint64;
        default:
            return false;
        }

    case RBH_FOP_LOWER_OR_EQUAL:
        switch (field_val->type) {
        case RBH_VT_INT32:
            return field_val->int32 <= filter_val->int32;
        case RBH_VT_UINT32:
            return field_val->uint32 <= filter_val->uint32;
        case RBH_VT_INT64:
            return field_val->int64 <= filter_val->int64;
        case RBH_VT_UINT64:
            return field_val->uint64 <= filter_val->uint64;
        default:
            return false;
        }

    case RBH_FOP_GREATER_OR_EQUAL:
        switch (field_val->type) {
        case RBH_VT_INT32:
            return field_val->int32 >= filter_val->int32;
        case RBH_VT_UINT32:
            return field_val->uint32 >= filter_val->uint32;
        case RBH_VT_INT64:
            return field_val->int64 >= filter_val->int64;
        case RBH_VT_UINT64:
            return field_val->uint64 >= filter_val->uint64;
        default:
            return false;
        }

    default:
        return false;
    }
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

bool
rbh_filter_matches_fsentry(const struct rbh_filter *filter,
                           const struct rbh_fsentry *fsentry)
{
    if (filter == NULL)
        return true;

    switch (filter->op) {
    case RBH_FOP_EQUAL:
    case RBH_FOP_STRICTLY_LOWER:
    case RBH_FOP_LOWER_OR_EQUAL:
    case RBH_FOP_STRICTLY_GREATER:
    case RBH_FOP_GREATER_OR_EQUAL:
        {
            const struct rbh_value *field_val =
                get_field_value(fsentry, &filter->compare.field);

            if (field_val == NULL)
                return false;

            return compare_values(filter->op, field_val,
                                 &filter->compare.value);
        }

    /* REGEX operator - for Name, Path with wildcards */
    case RBH_FOP_REGEX:
        {
            const struct rbh_value *field_val;
            unsigned int regex_options;
            const char *regex_string;
            regex_t compiled;
            int cflags;
            bool match;

            field_val = get_field_value(fsentry, &filter->compare.field);

            if (field_val == NULL || field_val->type != RBH_VT_STRING)
                return false;

            if (filter->compare.value.type != RBH_VT_REGEX)
                return false;

            regex_string = filter->compare.value.regex.string;
            regex_options = filter->compare.value.regex.options;

            cflags = REG_EXTENDED | REG_NOSUB;

            if (regex_options & RBH_RO_CASE_INSENSITIVE)
                cflags |= REG_ICASE;

            if (regex_options & RBH_RO_SHELL_PATTERN) {
                int flags = 0;
                if (regex_options & RBH_RO_CASE_INSENSITIVE)
                    flags |= FNM_CASEFOLD;
                return fnmatch(regex_string, field_val->string, flags) == 0;
            }

            if (regcomp(&compiled, regex_string, cflags) != 0)
                return false;

            match = regexec(&compiled, field_val->string, 0, NULL, 0) == 0;
            regfree(&compiled);
            return match;
        }

    /* IN operator - for User/Group with lists */
    case RBH_FOP_IN:
        {
            const struct rbh_value *seq_values;
            const struct rbh_value *field_val;
            size_t seq_count;

            field_val = get_field_value(fsentry, &filter->compare.field);

            if (field_val == NULL)
                return false;

            if (filter->compare.value.type != RBH_VT_SEQUENCE)
                return false;

            seq_values = filter->compare.value.sequence.values;
            seq_count = filter->compare.value.sequence.count;

            for (size_t i = 0; i < seq_count; i++) {
                if (compare_values(RBH_FOP_EQUAL, field_val, &seq_values[i]))
                    return true;
            }
            return false;
        }

    case RBH_FOP_EXISTS:
        {
            const struct rbh_value *field_val =
                get_field_value(fsentry, &filter->compare.field);
            return field_val != NULL;
        }

    case RBH_FOP_AND:
        for (size_t i = 0; i < filter->logical.count; i++) {
            if (!rbh_filter_matches_fsentry(filter->logical.filters[i],
                                            fsentry))
                return false;
        }
        return true;

    case RBH_FOP_OR:
        for (size_t i = 0; i < filter->logical.count; i++) {
            if (rbh_filter_matches_fsentry(filter->logical.filters[i],
                                           fsentry))
                return true;
        }
        return false;

    case RBH_FOP_NOT:
        return !rbh_filter_matches_fsentry(filter->logical.filters[0],
                                           fsentry);

    default:
        return false;
    }
}

struct rbh_fsentry *
rbh_get_fresh_fsentry(struct rbh_backend *backend,
                      struct rbh_fsentry *fsentry)
{
    struct rbh_filter_projection projection = {
        .fsentry_mask = RBH_FP_ALL,
        .statx_mask = RBH_STATX_ALL,
    };
    struct rbh_backend *backend_branch;
    struct rbh_fsentry *system_fsentry;

    backend_branch = rbh_backend_branch(backend, &fsentry->id, NULL);
    if (!backend_branch)
        return NULL;

    system_fsentry = rbh_backend_root(backend_branch, &projection);
    if (!system_fsentry)
        return NULL;

    rbh_backend_destroy(backend_branch);

    return system_fsentry;
}

int
rbh_check_real_fsentry_match_filter(struct rbh_backend *backend,
                                    const struct rbh_filter *filter,
                                    struct rbh_fsentry *fsentry)
{
    struct rbh_fsentry *system_fsentry;

    system_fsentry = rbh_get_fresh_fsentry(backend, fsentry);
    if (!system_fsentry)
        return 1;

    if (!rbh_filter_matches_fsentry(filter, system_fsentry)) {
        errno = EINVAL;
        return 1;
    }

    return 0;
}
