/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "robinhood/fsentry.h"
#include "robinhood/projection.h"
#include "robinhood/statx.h"

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
        // TODO: handle subfields
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
