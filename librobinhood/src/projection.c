/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <error.h>

#include "robinhood/fsentry.h"
#include "robinhood/projection.h"
#include "robinhood/statx.h"
#include "robinhood/sstack.h"
#include "robinhood/value.h"

static __thread struct rbh_sstack *sstack;

__attribute__((destructor))
static void
destroy_sstack(void)
{
    if (sstack)
        rbh_sstack_destroy(sstack);
}

void
rbh_projection_add(struct rbh_filter_projection *projection,
                   const struct rbh_filter_field *field)
{
    projection->fsentry_mask |= field->fsentry;

    switch (field->fsentry) {
    case RBH_FP_ID:
    case RBH_FP_PARENT_ID:
    case RBH_FP_NAME:
        break;
    case RBH_FP_STATX:
        projection->statx_mask |= field->statx;
        break;
    case RBH_FP_SYMLINK:
    case RBH_FP_NAMESPACE_XATTRS:
        // TODO: handle subfields
        break;
    case RBH_FP_INODE_XATTRS:
        if (field->xattr) {
            if (sstack == NULL)
                sstack = rbh_sstack_new((1 << 10) *
                                        (sizeof(struct rbh_value_pair)));

            struct rbh_value_pair pair = {
                .key = RBH_SSTACK_PUSH(sstack, field->xattr,
                                       strlen(field->xattr) + 1),
                .value = NULL
            };

            if (projection->xattrs.inode.pairs == NULL) {
                projection->xattrs.inode.pairs = RBH_SSTACK_PUSH(sstack, &pair,
                                                                 sizeof(pair));
                projection->xattrs.inode.count++;
            } else {
                value_map_insert_pair(sstack, &projection->xattrs.inode, &pair);
            }
        }

        break;
    }
}

void
rbh_projection_remove(struct rbh_filter_projection *projection,
                      const struct rbh_filter_field *field)
{
    projection->fsentry_mask &= ~field->fsentry;

    switch (field->fsentry) {
    case RBH_FP_ID:
    case RBH_FP_PARENT_ID:
    case RBH_FP_NAME:
        break;
    case RBH_FP_STATX:
        projection->statx_mask &= ~field->statx;
        if (projection->statx_mask & RBH_STATX_ALL)
            projection->fsentry_mask |= RBH_FP_STATX;
        break;
    case RBH_FP_SYMLINK:
    case RBH_FP_NAMESPACE_XATTRS:
        // TODO: handle subfields
        break;
    case RBH_FP_INODE_XATTRS:
        // TODO: handle subfields
        break;
    }
}

void
rbh_projection_set(struct rbh_filter_projection *projection,
                   const struct rbh_filter_field *field)
{
    projection->fsentry_mask = field->fsentry;
    projection->statx_mask = 0;
    projection->xattrs.inode.count = 0;
    projection->xattrs.ns.count = 0;

    switch (field->fsentry) {
    case RBH_FP_ID:
    case RBH_FP_PARENT_ID:
    case RBH_FP_NAME:
        break;
    case RBH_FP_STATX:
        projection->statx_mask = field->statx;
        break;
    case RBH_FP_SYMLINK:
    case RBH_FP_NAMESPACE_XATTRS:
        // TODO: handle subfields
        break;
    case RBH_FP_INODE_XATTRS:
        // TODO: handle subfields
        break;
    }
}
