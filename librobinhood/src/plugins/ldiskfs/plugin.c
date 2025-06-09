/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static const struct rbh_backend_plugin_operations
LDISKFS_BACKEND_PLUGIN_OPS = {
    .new = rbh_ldiskfs_backend_new,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(LDISKFS) = {
    .plugin = {
        .name    = RBH_LDISKFS_BACKEND_NAME,
        .version = RBH_LDISKFS_BACKEND_VERSION,
    },
    .ops = &LDISKFS_BACKEND_PLUGIN_OPS,
    .capabilities = RBH_FILTER_OPS | RBH_SYNC_OPS | RBH_UPDATE_OPS |
        RBH_BRANCH_OPS,
};
