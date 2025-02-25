/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "robinhood/backends/lustre_mpi.h"
#include "robinhood/plugins/backend.h"
#include "robinhood/backends/iter_mpi_internal.h"
#include "robinhood/plugin.h"

static const struct rbh_backend_plugin_operations
LUSTRE_MPI_BACKEND_PLUGIN_OPS = {
    .new = rbh_lustre_mpi_backend_new,
    .destroy = rbh_mpi_plugin_destroy,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(LUSTRE_MPI) = {
    .plugin = {
        .name = RBH_LUSTRE_MPI_BACKEND_NAME,
        .version = RBH_LUSTRE_MPI_BACKEND_VERSION,
    },
    .ops = &LUSTRE_MPI_BACKEND_PLUGIN_OPS,
    .capabilities = RBH_SYNC_OPS | RBH_BRANCH_OPS,
    .info = 0,
};

