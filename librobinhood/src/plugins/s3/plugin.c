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

static const struct rbh_backend_plugin_operations S3_BACKEND_PLUGIN_OPS = {
    .new = rbh_s3_backend_new,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(S3) = {
    .plugin = {
        .name = RBH_S3_BACKEND_NAME,
        .version = RBH_S3_BACKEND_VERSION,
    },
    .ops = &S3_BACKEND_PLUGIN_OPS,
    .capabilities = RBH_SYNC_OPS,
};
