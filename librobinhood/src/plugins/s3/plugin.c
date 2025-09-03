/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "robinhood/backends/s3.h"
#include "robinhood/plugins/backend.h"
#include "robinhood/plugin.h"

#include "s3_internals.h"

static const struct rbh_pe_common_operations S3_BACKEND_PLUGIN_COMMON_OPS = {
    .check_valid_token = rbh_s3_check_valid_token,
    .build_filter = rbh_s3_build_filter,
    .delete_entry = rbh_s3_delete_entry,
    .fill_entry_info = rbh_s3_fill_entry_info,
    .fill_projection = rbh_s3_fill_projection,
    .helper = rbh_s3_helper,
};

static const struct rbh_backend_plugin_operations S3_BACKEND_PLUGIN_OPS = {
    .new = rbh_s3_backend_new,
    .init = rbh_s3_plugin_init,
    .destroy = rbh_s3_plugin_destroy,
    .load_iterator = rbh_s3_backend_load_iterator,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(S3) = {
    .plugin = {
        .name = RBH_S3_BACKEND_NAME,
        .version = RBH_S3_BACKEND_VERSION,
    },
    .ops = &S3_BACKEND_PLUGIN_OPS,
    .common_ops = &S3_BACKEND_PLUGIN_COMMON_OPS,
    .capabilities = RBH_SYNC_OPS,
};
