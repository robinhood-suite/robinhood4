/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood/backends/s3.h>
#include <robinhood/backends/s3_mpi.h>
#include <robinhood/plugins/backend.h>

const struct rbh_s3_extension RBH_BACKEND_EXTENDS(S3, MPI) = {
    .extension = {
        .super       = RBH_S3_BACKEND_NAME,
        .name        = RBH_S3_MPI_PLUGIN_NAME,
        .version     = RBH_S3_MPI_PLUGIN_VERSION,
        .min_version = RBH_S3_BACKEND_VERSION,
        .max_version = RBH_S3_BACKEND_VERSION,
    },
    .iter_new = rbh_s3_mpi_iter_new,
};
