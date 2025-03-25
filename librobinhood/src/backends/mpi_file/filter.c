/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <error.h>
#include <fnmatch.h>

#include "mfu.h"
#include "robinhood/statx.h"
#include "robinhood/value.h"
#include "robinhood/filter.h"
#include "mpi_file.h"

static mfu_pred_fn
statx2mfu_fn(const uint32_t statx)
{
    switch(statx)
    {
    case RBH_STATX_ATIME:
    case RBH_STATX_ATIME_SEC:
    case RBH_STATX_ATIME_NSEC:
        return MFU_PRED_AMIN;

    case RBH_STATX_CTIME:
    case RBH_STATX_CTIME_SEC:
    case RBH_STATX_CTIME_NSEC:
        return MFU_PRED_CMIN;

    case RBH_STATX_MTIME:
    case RBH_STATX_MTIME_SEC:
    case RBH_STATX_MTIME_NSEC:
        return MFU_PRED_MMIN;

    case RBH_STATX_TYPE:
        return MFU_PRED_TYPE;

    case RBH_STATX_SIZE:
        return _MFU_PRED_SIZE;

    default:
        errno = ENOTSUP;
        return NULL;
    }
}

static mfu_pred_fn
filter2mfu_fn(const struct rbh_filter *filter)
{
    struct rbh_filter_field field = filter->compare.field;

    switch(field.fsentry)
    {
    case RBH_FP_NAME:
        return MFU_PRED_NAME;
    case RBH_FP_STATX:
        return statx2mfu_fn(field.statx);
    case RBH_FP_NAMESPACE_XATTRS:
        if (!strcmp(field.xattr, "path"))
            return _MFU_PRED_PATH;
        break;
    default:
        break;
    }

    errno = ENOTSUP;
    return NULL;
}

static struct rbh_value *
shell_pattern2map(int prefix_len, const char *pattern) {
    const struct rbh_value pattern_val = {
        .type = RBH_VT_STRING,
        .string = pattern,
    };
    const struct rbh_value prefix_val = {
        .type = RBH_VT_INT32,
        .int32 = prefix_len,
    };
    struct rbh_value_pair *pairs;
    struct rbh_value_pair *pair;
    struct rbh_value *map;

    /* It will be free by mfu_pred_free() */
    pairs = reallocarray(NULL, 2, sizeof(*pairs));
    if (pairs == NULL)
        error(EXIT_FAILURE, ENOMEM, "reallocarray rbh_value_pair");

    pair = &pairs[0];
    pair->key = "pattern";
    pair->value = &pattern_val;

    pair = &pair[1];
    pair->key = "prefix_len";
    pair->value = &prefix_val;

    map = rbh_value_map_new(pairs, 2);
    if (map == NULL)
        error(EXIT_FAILURE, ENOMEM, "rbh_value_map_new");

    return map;
}

static void *
filter2arg(mfu_pred_times *now, const struct rbh_filter *filter, int prefix_len)
{
    const struct rbh_filter_field field = filter->compare.field;
    const struct rbh_value *value = &filter->compare.value;
    struct rbh_filter *arg_filter;
    mode_t *arg_type;
    char *arg_regex;

    switch(field.fsentry)
    {
    case RBH_FP_NAME:
        /* It will be free by mfu_pred_free() */
        switch(value->type)
        {
        case RBH_VT_STRING:
            arg_regex = strdup(value->string);
            break;
        case RBH_VT_REGEX:
            arg_regex = strdup(value->regex.string);
            break;
        default:
            errno = ENOTSUP;
            return NULL;
        }

        if (arg_regex == NULL)
            error(EXIT_FAILURE, errno, "strdup name argument");
        return arg_regex;

    case RBH_FP_NAMESPACE_XATTRS:
        if (strcmp(field.xattr, "path")) {
            errno = ENOTSUP;
            return NULL;
        }

        switch(value->type)
        {
        case RBH_VT_STRING:
            return shell_pattern2map(prefix_len, value->string);
        case RBH_VT_REGEX:
            return shell_pattern2map(prefix_len, value->regex.string);
        default:
            errno = ENOTSUP;
            return NULL;
        }

    case RBH_FP_STATX:
        switch(field.statx)
        {
        case RBH_STATX_TYPE:
            /* It will be free by mfu_pred_free() */
            arg_type = malloc(sizeof(*arg_type));
            if (arg_type == NULL)
                error(EXIT_FAILURE, errno, "malloc mode_t type");

            *arg_type = value->uint32;
            return arg_type;

        case RBH_STATX_SIZE:
            /* We return the filter because our new _MFU_PRED_SIZE function
             * take it as argument.
             * It will be free by mfu_pred_free() */
            arg_filter = rbh_filter_clone(filter);
            if (arg_filter == NULL)
                error(EXIT_FAILURE, errno, "clone arg_filter");

            return arg_filter;

        case RBH_STATX_ATIME:
        case RBH_STATX_ATIME_SEC:
        case RBH_STATX_ATIME_NSEC:
        case RBH_STATX_CTIME:
        case RBH_STATX_CTIME_SEC:
        case RBH_STATX_CTIME_NSEC:
        case RBH_STATX_MTIME:
        case RBH_STATX_MTIME_SEC:
        case RBH_STATX_MTIME_NSEC:
            return _mfu_pred_relative(filter, now);

        default:
            errno = ENOTSUP;
            return NULL;
        }

    default:
        errno = ENOTSUP;
        return NULL;
    }
}

static bool
convert_comparison_filter(mfu_pred *pred, mfu_pred_times *now, int prefix_len,
                          const struct rbh_filter *filter)
{
    mfu_pred_fn func;
    void *arg;

    func = filter2mfu_fn(filter);
    if (func == NULL)
        return false;

    arg = filter2arg(now, filter, prefix_len);
    if (arg == NULL)
        return false;

    mfu_pred_add(pred, func, arg);

    return true;
}

static bool
create_mfu_pred_and_or(mfu_pred *curr, mfu_pred_times *now, int prefix_len,
                       const struct rbh_filter *filter,
                       mfu_pred_fn logical_func)
{
    mfu_pred *pred = mfu_pred_new();

    for (uint32_t i = 0; i < filter->logical.count; i++) {
        if (!convert_rbh_filter(pred, now, prefix_len,
                                filter->logical.filters[i]))
            return false;
    }

    mfu_pred_add(curr, logical_func, pred);
    return true;
}

static bool
create_mfu_pred_not(mfu_pred *curr, mfu_pred_times *now, int prefix_len,
                    const struct rbh_filter *filter)
{
    mfu_pred *not = mfu_pred_new();

    if (!convert_rbh_filter(not, now, prefix_len, filter->logical.filters[0]))
        return false;

    mfu_pred_add(curr, _MFU_PRED_NOT, not);
    return true;
}

static bool
convert_logical_filter(mfu_pred *pred, mfu_pred_times *now, int prefix_len,
                       const struct rbh_filter *filter)
{
    switch(filter->op)
    {
    case RBH_FOP_AND:
        return create_mfu_pred_and_or(pred, now, prefix_len, filter,
                                      _MFU_PRED_AND);
    case RBH_FOP_NOT:
        return create_mfu_pred_not(pred, now, prefix_len, filter);
    case RBH_FOP_OR:
        return create_mfu_pred_and_or(pred, now, prefix_len, filter,
                                      _MFU_PRED_OR);
    default:
        errno = ENOTSUP;
        return false;
    }
}

bool
convert_rbh_filter(mfu_pred *pred, mfu_pred_times *now, int prefix_len,
                   const struct rbh_filter *filter)
{
    if (filter == NULL) {
        mfu_pred_add(pred, _MFU_PRED_NULL, NULL);
        return true;
    }

    if (rbh_is_comparison_operator(filter->op))
        return convert_comparison_filter(pred, now, prefix_len, filter);
    return convert_logical_filter(pred, now, prefix_len, filter);
}

mfu_pred *
rbh_filter2mfu_pred(const struct rbh_filter *filter, int prefix_len,
                    mfu_pred_times *now)
{
    mfu_pred *pred_head = mfu_pred_new();
    bool rc = false;

    rc = convert_rbh_filter(pred_head, now, prefix_len, filter);

    return rc ? pred_head : NULL;
}

enum mfu_pred_type {
    MFU_LOGICAL,
    MFU_COMPARISON
};

static enum mfu_pred_type
check_mfu_pred_type(mfu_pred *pred)
{
    if (pred->f == _MFU_PRED_AND || pred->f == _MFU_PRED_NOT ||
        pred->f == _MFU_PRED_OR)
        return MFU_LOGICAL;
    else
        return MFU_COMPARISON;
}

void
_mfu_pred_free(mfu_pred *pred)
{
    enum mfu_pred_type type;
    mfu_pred *cur = pred;

    while (cur) {
        mfu_pred *next = cur->next;
        if (cur->arg != NULL) {
            type = check_mfu_pred_type(cur);
            if (type == MFU_LOGICAL)
                _mfu_pred_free(cur->arg);
            else
                mfu_free(&cur->arg);
        }
        mfu_free(&cur);
        cur = next;
    }
}
