/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "robinhood/backends/lustre.h"
#include "robinhood/plugins/backend.h"
#include "robinhood/plugin.h"

static const struct rbh_backend_plugin_operations LUSTRE_BACKEND_PLUGIN_OPS = {
    .new = rbh_lustre_backend_new,
    .check_valid_token = rbh_lustre_check_valid_token,
    .build_filter = rbh_lustre_build_filter,
    .fill_entry_info = rbh_lustre_fill_entry_info,
    .delete_entry = rbh_lustre_delete_entry,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(LUSTRE) = {
    .plugin = {
        .name = RBH_LUSTRE_BACKEND_NAME,
        .version = RBH_LUSTRE_BACKEND_VERSION,
    },
    .ops = &LUSTRE_BACKEND_PLUGIN_OPS,
    .capabilities = RBH_SYNC_OPS | RBH_BRANCH_OPS,
    .info = 0,
};
