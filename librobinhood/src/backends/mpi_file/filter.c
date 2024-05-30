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

/**
 * We have redefined mfu_pred_relative and MFU_PRED_SIZE from mpifileutils
 * to be able to use the rbh_filter structure in argument.
 *
 * Originally, these two functions in mpifileutils take in an argument
 * a char *: "(+/-)N". By using a rbh_filter structure as an argument, we avoid
 * recreating a char * which has already been parsed by rbh-find.
 */

/**
 * _mfu_pred_relative is an intermediate function whose return value is used
 * by MFU_PRED_ATIME, MFU_PRED_CTIME and MFU_PRED_MTIME.
 */
static mfu_pred_times_rel *
_mfu_pred_relative(const struct rbh_filter *filter)
{
    const struct rbh_value *value = &filter->compare.value;
    mfu_pred_times *now = mfu_pred_now();
    int cmp;

    switch(filter->op)
    {
    case RBH_FOP_STRICTLY_GREATER:
        cmp = 1;
        break;
    case RBH_FOP_STRICTLY_LOWER:
        cmp = -1;
        break;
    case RBH_FOP_EQUAL:
        cmp = 0;
        break;
    default:
        errno = ENOTSUP;
        return NULL;
    }

    mfu_pred_times_rel *r = (mfu_pred_times_rel *)
                             MFU_MALLOC(sizeof(mfu_pred_times_rel));
    r->magnitude = value->uint64;
    r->t.nsecs = now->nsecs;
    r->t.secs = now->secs;
    r->direction = cmp;
    mfu_free(&now);

    return r;
}

static int
_MFU_PRED_SIZE(mfu_flist flist, uint64_t idx, void *arg)
{
    struct rbh_filter *filter = (struct rbh_filter *) arg;
    uint64_t size = mfu_flist_file_get_size(flist, idx);
    struct rbh_value *value = &filter->compare.value;
    uint64_t bytes = value->uint64;
    int ret = 0;

    switch(filter->op)
    {
    case RBH_FOP_STRICTLY_GREATER:
        if (size > bytes)
            ret = 1;
        break;
    case RBH_FOP_GREATER_OR_EQUAL:
        if (size >= bytes)
            ret = 1;
        break;
    case RBH_FOP_STRICTLY_LOWER:
        if (size < bytes)
            ret = 1;
        break;
    case RBH_FOP_LOWER_OR_EQUAL:
        if (size <= bytes)
            ret = 1;
        break;
    case RBH_FOP_EQUAL:
        if (size == bytes)
            ret = 1;
        break;
    default:
        errno = ENOTSUP;
        ret = 0;
        break;
    }
    return ret;
}

static int
_MFU_PRED_PATH(mfu_flist flist, uint64_t idx, void *arg)
{
    const char *name = mfu_flist_file_get_name(flist, idx);
    struct rbh_value *value = (struct rbh_value *) arg;
    struct rbh_value_map map = value->map;
    struct rbh_value_pair pair;
    const char *pattern;
    int prefix_len;
    int ret = 0;
    char *path;

    pair = map.pairs[0];
    pattern = pair.value->string;

    pair = map.pairs[1];
    prefix_len = pair.value->int32;

    path = strlen(name) == prefix_len ? "/" : (char *) name + prefix_len;
    ret = fnmatch(pattern, path, FNM_PERIOD) ? 0 : 1;
    return ret;
}

static mfu_pred_fn
statx2mfu_fn(const uint32_t statx)
{
    switch(statx)
    {
    case RBH_STATX_ATIME:
    case RBH_STATX_ATIME_SEC:
    case RBH_STATX_ATIME_NSEC:
        return MFU_PRED_ATIME;

    case RBH_STATX_CTIME:
    case RBH_STATX_CTIME_SEC:
    case RBH_STATX_CTIME_NSEC:
        return MFU_PRED_CTIME;

    case RBH_STATX_MTIME:
    case RBH_STATX_MTIME_SEC:
    case RBH_STATX_MTIME_NSEC:
        return MFU_PRED_MTIME;

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
filter2arg(const struct rbh_filter *filter, int prefix_len)
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
            return _mfu_pred_relative(filter);

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
convert_comparison_filter(mfu_pred *pred, int prefix_len,
                          const struct rbh_filter *filter)
{
    mfu_pred_fn func;
    void *arg;

    func = filter2mfu_fn(filter);
    if (func == NULL)
        return false;

    arg = filter2arg(filter, prefix_len);
    if (arg == NULL)
        return false;

    mfu_pred_add(pred, func, arg);

    return true;
}

static bool
convert_logical_filter(mfu_pred *pred, int prefix_len,
                       const struct rbh_filter *filter)
{
    if (filter->op == RBH_FOP_OR || filter->op == RBH_FOP_NOT) {
        printf("Logical operator OR and NOT are not supported.\n");
        errno = ENOTSUP;
        return false;
    }

    for(uint32_t i = 0; i < filter->logical.count; i++) {
        if (!convert_rbh_filter(pred, prefix_len, filter->logical.filters[i]))
            return false;
    }

    return true;
}

bool
convert_rbh_filter(mfu_pred *pred, int prefix_len,
                   const struct rbh_filter *filter)
{
    if (filter == NULL)
        return true;

    if (rbh_is_comparison_operator(filter->op))
        return convert_comparison_filter(pred, prefix_len, filter);
    return convert_logical_filter(pred, prefix_len, filter);
}

mfu_pred *
rbh_filter2mfu_pred(const struct rbh_filter *filter, int prefix_len)
{
    mfu_pred *pred_head = mfu_pred_new();
    bool rc = false;

    rc = convert_rbh_filter(pred_head, prefix_len, filter);

    return rc ? pred_head : NULL;
}
