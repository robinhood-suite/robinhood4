/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "robinhood/backends/mongo.h"
#include "robinhood/plugins/backend.h"
#include "robinhood/plugin.h"

static const struct rbh_backend_plugin_operations MONGO_BACKEND_PLUGIN_OPS = {
    .new = rbh_mongo_backend_new,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(MONGO) = {
    .plugin = {
        .name = RBH_MONGO_BACKEND_NAME,
        .version = RBH_MONGO_BACKEND_VERSION,
    },
    .ops = &MONGO_BACKEND_PLUGIN_OPS,
    .capabilities = RBH_FILTER_OPS | RBH_SYNC_OPS | RBH_UPDATE_OPS |
                    RBH_BRANCH_OPS,
    .info = RBH_INFO_SIZE | RBH_INFO_COUNT | RBH_INFO_AVG_OBJ_SIZE |
            RBH_INFO_BACKEND_SOURCE,
};
