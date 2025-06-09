/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

struct rbh_mut_iterator *
ldiskfs_backend_filter(void *backend, const struct rbh_filter *filter,
                       const struct rbh_filter_options *options,
                       const struct rbh_filter_output *output)
{
    struct ldiskfs_backend *ldiskfs = backend;
    struct rbh_dcache_iter *iter;

    if (!scan_target(ldiskfs))
        return NULL;

    iter = rbh_dcache_iter_new(ldiskfs->dcache);
    if (!iter)
        return NULL;

    return &iter->iter;
}
