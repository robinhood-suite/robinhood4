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

static const struct rbh_backend_plugin_operations
LUSTRE_MPI_BACKEND_PLUGIN_OPS = {
    .new = rbh_lustre_mpi_backend_new,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(LUSTRE_MPI) = {
    .plugin = {
        .name = RBH_LUSTRE_MPI_BACKEND_NAME,
        .version = RBH_LUSTRE_MPI_BACKEND_VERSION,
    },
    .ops = &LUSTRE_MPI_BACKEND_PLUGIN_OPS,
};

