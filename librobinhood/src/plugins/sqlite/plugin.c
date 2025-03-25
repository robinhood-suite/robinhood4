/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/backends/sqlite.h"
#include "robinhood/plugins/backend.h"

static const struct rbh_backend_plugin_operations SQLITE_BACKEND_PLUGIN_OPS = {
    .new = rbh_sqlite_backend_new,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(SQLITE) = {
    .plugin = {
        .name    = RBH_SQLITE_BACKEND_NAME,
        .version = RBH_SQLITE_BACKEND_VERSION,
    },
    .ops = &SQLITE_BACKEND_PLUGIN_OPS,
    .capabilities = RBH_FILTER_OPS | RBH_SYNC_OPS | RBH_UPDATE_OPS | RBH_BRANCH_OPS,
};
