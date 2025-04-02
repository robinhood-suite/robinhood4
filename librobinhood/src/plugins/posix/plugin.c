/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/backends/posix.h"
#include "robinhood/plugins/backend.h"
#include "robinhood/plugin.h"

static const struct rbh_pe_common_operations POSIX_BACKEND_PLUGIN_COMMON_OPS = {
    .check_valid_token = rbh_posix_check_valid_token,
    .build_filter = rbh_posix_build_filter,
    .fill_entry_info = rbh_posix_fill_entry_info,
    .delete_entry = rbh_posix_delete_entry,
};

static const struct rbh_backend_plugin_operations POSIX_BACKEND_PLUGIN_OPS = {
    .new = rbh_posix_backend_new,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(POSIX) = {
    .plugin = {
        .name = RBH_POSIX_BACKEND_NAME,
        .version = RBH_POSIX_BACKEND_VERSION,
    },
    .ops = &POSIX_BACKEND_PLUGIN_OPS,
    .common_ops = &POSIX_BACKEND_PLUGIN_COMMON_OPS,
    .capabilities = RBH_SYNC_OPS | RBH_BRANCH_OPS,
    .info = 0,
};
