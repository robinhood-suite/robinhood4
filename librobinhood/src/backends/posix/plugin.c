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

#include "robinhood/backends/posix.h"
#include "robinhood/plugins/backend.h"

static const struct rbh_backend_plugin_operations POSIX_BACKEND_PLUGIN_OPS = {
    .new = rbh_posix_backend_new,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(POSIX) = {
    .plugin = {
        .name = RBH_POSIX_BACKEND_NAME,
        .version = RBH_POSIX_BACKEND_VERSION,
    },
    .ops = &POSIX_BACKEND_PLUGIN_OPS,
};
