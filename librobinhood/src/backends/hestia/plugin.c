/* This file is part of Robinhood 4
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "robinhood/backends/hestia.h"
#include "robinhood/plugins/backend.h"
#include "robinhood/plugin.h"

static const struct rbh_backend_plugin_operations HESTIA_BACKEND_PLUGIN_OPS = {
    .new = rbh_hestia_backend_new,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(HESTIA) = {
    .plugin = {
        .name = RBH_HESTIA_BACKEND_NAME,
        .version = RBH_HESTIA_BACKEND_VERSION,
    },
    .ops = &HESTIA_BACKEND_PLUGIN_OPS,
    .info = RBH_SYNC_OPS,
};
