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
#include <sys/types.h>
#include <regex.h>
#include <error.h>

#include "mfu.h"
#include "robinhood/statx.h"
#include "robinhood/value.h"
#include "robinhood/filter.h"

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
_mfu_pred_relative(const struct rbh_filter *filter, const mfu_pred_times *t)
{
    const struct rbh_value *value = &filter->compare.value;
    uint64_t val = value->uint64;
    uint64_t magnitude;
    int cmp;

    switch(filter->op)
    {
    case RBH_FOP_STRICTLY_GREATER:
        cmp = 1;
        magnitude = val;
        break;
    case RBH_FOP_STRICTLY_LOWER:
        cmp = -1;
        magnitude = val;
        break;
    case RBH_FOP_EQUAL:
        cmp = 0;
        magnitude = val;
        break;
    default:
        errno = ENOTSUP;
        return NULL;
    }

    mfu_pred_times_rel *r = (mfu_pred_times_rel *)
                             MFU_MALLOC(sizeof(mfu_pred_times_rel));
    r->direction = cmp;
    r->magnitude = magnitude;
    r->t.secs = t->secs;
    r->t.nsecs = t->nsecs;

    return r;
}

static int
_MFU_PRED_SIZE(mfu_flist flist, uint64_t idx, void *arg)
{
    struct rbh_filter *filter = (struct rbh_filter *) arg;
    struct rbh_value *value = &filter->compare.value;
    uint64_t size = mfu_flist_file_get_size(flist, idx);
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

static mfu_pred_fn
statx2mfu_fn(const uint32_t statx)
{
    switch(statx)
    {
    case RBH_STATX_ATIME_SEC:
        return MFU_PRED_ATIME;
    case RBH_STATX_CTIME_SEC:
        return MFU_PRED_CTIME;
    case RBH_STATX_MTIME_SEC:
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

__attribute__((unused))
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
            return MFU_PRED_PATH;
        break;
    default:
        break;
    }

    errno = ENOTSUP;
    return NULL;
}

__attribute__((unused))
static void *
filter2arg(mfu_pred_times *now, const struct rbh_filter *filter)
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
        arg_regex = strdup(value->regex.string);
        if (arg_regex == NULL)
            error(EXIT_FAILURE, errno, "strdup name argument");
        return arg_regex;
    case RBH_FP_NAMESPACE_XATTRS:
        if (!strcmp(field.xattr, "path")) {
            /* It will be free by mfu_pred_free() */
            arg_regex = strdup(value->regex.string);
            if (arg_regex == NULL)
                error(EXIT_FAILURE, errno, "strdup path argument");
            return arg_regex;
        }
        errno = ENOTSUP;
        return NULL;
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
        case RBH_STATX_ATIME_SEC:
        case RBH_STATX_CTIME_SEC:
        case RBH_STATX_MTIME_SEC:
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
