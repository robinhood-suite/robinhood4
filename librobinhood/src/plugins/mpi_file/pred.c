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

/*-----------------------------------------------------------------------------*
 |                             PRED FUNCTION                                   |
 *----------------------------------------------------------------------------*/

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
 * by MFU_PRED_AMIN, MFU_PRED_CMIN and MFU_PRED_MMIN.
 */

static uint64_t TU_MINUTE = 60;

mfu_pred_times_rel *
_mfu_pred_relative(const struct rbh_filter *filter, const mfu_pred_times *now)
{
    const struct rbh_value *value = &filter->compare.value;
    uint64_t magnitude;
    int cmp;

    /* We need to convert the time contained in seconds in the filter into
     * minutes relative to the current time to use mpifileutils's comparison
     * functions (MFU_PRED_AMIN,...).
     */
    magnitude = (now->secs - value->uint64) / TU_MINUTE;

    /* rbh-find check if an entry's time is bigger or smaller than the filter
     * with STRICTLY_GREATER and STRICTLY_LOWER. However, mpifileutils check if
     * an entry's time is smaller or bigger than the filter, so we have to
     * reverse the operators.
     */
    switch(filter->op)
    {
    case RBH_FOP_STRICTLY_GREATER:
        cmp = -1;
        if (magnitude == 0)
            cmp = 0;
        break;
    case RBH_FOP_STRICTLY_LOWER:
        cmp = 1;
        if (magnitude == 0)
            cmp = 0;
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
    r->magnitude = magnitude;
    r->t.nsecs = now->nsecs;
    r->t.secs = now->secs;
    r->direction = cmp;

    return r;
}

int
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
        break;
    }
    return ret;
}

int
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

/*-----------------------------------------------------------------------------*
 |                          PRED LOGICAL FUNCTIONS                             |
 *----------------------------------------------------------------------------*/

int
_MFU_PRED_AND(mfu_flist flist, uint64_t idx, void *arg)
{
    return mfu_pred_execute(flist, idx, (mfu_pred *) arg);
}

int
_MFU_PRED_NULL(mfu_flist flist, uint64_t idx, void *arg)
{
    return 1;
}

int
_MFU_PRED_NOT(mfu_flist flist, uint64_t idx, void *arg)
{
    return 1 - mfu_pred_execute(flist, idx, (mfu_pred *) arg);
}

int
_MFU_PRED_OR(mfu_flist flist, uint64_t idx, void *arg)
{
    mfu_pred *root = (mfu_pred *) arg;
    mfu_pred *cur = root;
    int rc;

    while (cur) {
        if (cur->f !=NULL) {
            rc = cur->f(flist, idx, cur->arg);
            if (rc)
                return 1;
        }
        cur = cur->next;
    }
    return 0;
}
