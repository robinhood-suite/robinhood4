/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "backends/posix.h"
#include "sparse_internals.h"
#include "robinhood/backends/sparse.h"
#include "robinhood/backends/posix_extension.h"
#include "robinhood/plugins/backend.h"

static const struct rbh_pe_common_operations SPARSE_EXTENSION_COMMON_OPS = {
    .check_valid_token = rbh_sparse_check_valid_token,
    .build_filter = rbh_sparse_build_filter,
    .fill_entry_info = rbh_sparse_fill_entry_info,
    .fill_projection = rbh_sparse_fill_projection,
};

const struct rbh_posix_extension RBH_BACKEND_EXTENDS(POSIX, SPARSE) = {
    .extension = {
        .super       = RBH_POSIX_BACKEND_NAME,
        .name        = RBH_SPARSE_PLUGIN_NAME,
        .version     = RBH_SPARSE_PLUGIN_VERSION,
        .min_version = RBH_POSIX_BACKEND_VERSION,
        .max_version = RBH_POSIX_BACKEND_VERSION,
        .common_ops  = &SPARSE_EXTENSION_COMMON_OPS,
    },
    .enrich         = rbh_sparse_enrich,
    .setup_enricher = rbh_sparse_setup,
};
