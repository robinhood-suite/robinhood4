/* This file is part of RobinHood 4
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "robinhood/backends/lustre.h"
#include "lustre_internals.h"
#include "robinhood/backends/posix.h"
#include "robinhood/backends/posix_extension.h"
#include "robinhood/plugins/backend.h"
#include "robinhood/plugin.h"

static const struct rbh_backend_plugin_operations LUSTRE_BACKEND_PLUGIN_OPS = {
    .new = rbh_lustre_backend_new,
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

const struct rbh_posix_extension RBH_BACKEND_EXTENDS(POSIX, LUSTRE) = {
    .extension = {
        .super       = RBH_POSIX_BACKEND_NAME,
        .name        = RBH_LUSTRE_BACKEND_NAME,
        .version     = RBH_LUSTRE_BACKEND_VERSION,
        .min_version = RBH_POSIX_BACKEND_VERSION,
        .max_version = RBH_POSIX_BACKEND_VERSION,
    },
    .enrich = rbh_lustre_enrich,
};
