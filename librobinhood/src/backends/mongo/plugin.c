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

static const struct rbh_backend_plugin_operations MONGO_BACKEND_PLUGIN_OPS = {
    .new = rbh_mongo_backend_new,
    .destroy = rbh_mongo_backend_destroy,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(MONGO) = {
    .plugin = {
        .name = RBH_MONGO_BACKEND_NAME,
        .version = RBH_MONGO_BACKEND_VERSION,
    },
    .ops = &MONGO_BACKEND_PLUGIN_OPS,
};
