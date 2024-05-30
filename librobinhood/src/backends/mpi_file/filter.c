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

#include "robinhood/statx.h"
#include "mpi_file.h"

static void *
rbh_value_get(struct rbh_value *value)
{
    switch(value->type)
    {
    case RBH_VT_BOOLEAN:
        return value->boolean;
    case RBH_VT_INT32:
        return value->int32;
    case RBH_UINT32:
        return value->uint32;
    case RBH_VT_INT64:
        return value->int64;
    case RBH_VT_UINT64:
        return value->uint64;
    case RBH_VT_STRING:
        return value->string;
    case RBH_VT_REGEX:
        return value->regex.string;
    default:
        errno = ENOTSUP;
        return NULL;
    }
}

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
_mfu_pred_relative(struct rbh_filter *filter, const mfu_pred_times *t)
{
    struct rbh_value *value = &filter->compare.value;
    uint64_t magnitude;
    void *val;
    int cmp;

    val = rbh_value_get(value);
    if (val == NULL)
        return NULL;

    switch(filter->op)
    {
    case RBH_FOP_STRICTLY_GREATER:
        cmp = 1;
        magnitude = *(uint64_t *) val;
        break;
    case RBH_FOP_STRICTLY_LOWER:
        cmp = -1;
        magnitude = *(uint64_t *) val;
        break;
    case RBH_FOP_EQUAL:
        cmp = 0;
        magnitude = *(uint64_t *) val;
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
    uint64_t size = mfu_flist_file_get_size(flist, idx);
    struct rbh_value *value = &filter->compare.value;
    void *bytes;
    int ret = 0;

    bytes = rbh_value_get(value);
    if (bytes == NULL)
        return ret;

    switch(filter->op)
    {
    case RBH_FOP_STRICTLY_GREATER:
        if (size > *(uint64_t *) bytes)
            ret = 1;
        break;
    case RBH_FOP_STRICTLY_LOWER:
        if (size < *(uint64_t *) bytes)
            ret = 1;
        break;
    case RBH_FOP_EQUAL:
        if (size == *(uint64_t *) bytes)
            ret = 1;
        break;
    default:
        errno = ENOTSUP;
        ret = 0;
        break;
    }
    return ret;
}

static const struct mfu_pred_fn filter_field_statx2mfu_fn[] = {
    [RBH_STATX_ATIME_SEC] = MFU_PRED_ATIME,
    [RBH_STATX_CTIME_SEC] = MFU_PRED_CTIME,
    [RBH_STATX_MTIME_SEC] = MFU_PRED_MTIME,
    [RBH_STATX_TYPE]      = MFU_PRED_TYPE,
    [RBH_STATX_SIZE]      = _MFU_PRED_SIZE,
};

static mfu_pred_fn
filter2mfu_fn(struct rbh_filter *filter)
{
    struct rbh_filter_field field = filter->compare.field;

    switch(field.fsentry)
    {
    case RBH_FP_NAME:
        return MFU_PRED_NAME;
    case RBH_FP_STATX:
        return filter_field_statx2mfu_fn[field.statx];
    case RBH_FP_NAMESPACE_XATTRS:
        if (!strcmp(field.xattr, "path"))
            return MFU_PRED_PATH;
        break;
    default:
        break;
    }

    errno = ENOTSUP;
    fprintf(stderr, "Invalid predicate provided");
    return NULL;
}

static void *
filter2arg(mfu_pred_times *now, struct rbh_filter *filter)
{
    struct rbh_filter_field field = filter->compare.field;
    struct rbh_value *value = &filter->compare.value;

    switch(field.fsentry)
    {
    case RBH_FP_NAME:
        return rbh_value_get(value);
    case RBH_FP_NAMESPACE_XATTRS:
        if (!strcmp(field.xattr, "path"))
            return rbh_value_get(value);
        errno = ENOTSUP;
        return NULL;
    case RBH_FP_STATX:
        switch(field.statx)
        {
        case RBH_STATX_TYPE:
            return rbh_value_get(value);
        case RBH_STATX_SIZE:
            /* We return the filter because our new _MFU_PRED_SIZE function
             * take it as argument. */
            return filter;
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

static bool
convert_comparison_filter(mfu_pred *pred, mfu_pred_times *now,
                          struct rbh_filter *filter)
{
    mfu_pred_fn func;
    void *arg;

    func = filter2mfu_fn(filter);
    if (func == NULL)
        return false;

    arg = filter2arg(now, filter);
    if (arg == NULL)
        return false;

    mfu_pred_add(pred, func, arg);

    return true;
}

static bool
convert_logical_filter(mfu_pred *pred, mfu_pred_times *now,
                       struct rbh_filter *filter)
{
    if (filter->op == RBH_FOP_NOT || filter->op == RBH_FOP_NOT) {
        errno = ENOTSUP;
        fprintf(stderr, "Invalid operator provided, should only be 'AND'");
        return false;
    }

    for(uint32_t i = 0; i < filter->logical.count; i++) {
        if (!convert_rbh_filter(pred, filter->logical.filters[i]))
            return false;
    }

    return true;
}

bool
convert_rbh_filter(mfu_pred *pred, mfu_pred_times *now,
                   struct rbh_filter *filter)
{
    if (filter == NULL)
        return true;

    if (rbh_is_comparison_operator(filter->op))
        return convert_comparison_filter(pred, now, filter);
    return convert_logical_filter(pred, now, filter);
}

mfu_pred *
rbh_filter2mfu_pred(struct rbh_filter *filter)
{
    mfu_pred *pred_head = mfu_pred_new();
    mfu_pred_times *now = mfu_pred_now();
    bool rc = false;

    rc = convert_rbh_filter(pred_head, now, filter);
    mfu_free(&now);

    return rc ? pred_head : NULL;
}
