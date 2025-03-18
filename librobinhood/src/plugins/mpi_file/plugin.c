/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/backends/mpi_file.h"
#include "robinhood/plugins/backend.h"
#include "robinhood/plugin.h"
#include "mpi_file.h"

static const struct rbh_backend_plugin_operations
MPI_FILE_BACKEND_PLUGIN_OPS = {
    .new = rbh_mpi_file_backend_new,
    .destroy = rbh_mpi_file_plugin_destroy,
};

const struct rbh_backend_plugin RBH_BACKEND_PLUGIN_SYMBOL(MPI_FILE) = {
    .plugin = {
        .name = RBH_MPI_FILE_BACKEND_NAME,
        .version = RBH_MPI_FILE_BACKEND_VERSION,
    },
    .ops = &MPI_FILE_BACKEND_PLUGIN_OPS,
    .capabilities = RBH_SYNC_OPS | RBH_FILTER_OPS | RBH_UPDATE_OPS,
};

