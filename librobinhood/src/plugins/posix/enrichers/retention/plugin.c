/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "backends/posix.h"
#include "backends/lustre.h"
#include "retention_internals.h"
#include "robinhood/backends/retention.h"
#include "robinhood/backends/posix_extension.h"
#include "robinhood/plugins/backend.h"

static const struct rbh_pe_common_operations RETENTION_EXTENSION_COMMON_OPS = {
    .build_filter = rbh_retention_build_filter,
    .fill_entry_info = rbh_retention_fill_entry_info,
    .delete_entry = NULL,
};

const struct rbh_posix_extension RBH_BACKEND_EXTENDS(POSIX, RETENTION) = {
    .extension = {
        .super       = RBH_POSIX_BACKEND_NAME,
        .name        = RBH_RETENTION_PLUGIN_NAME,
        .version     = RBH_RETENTION_PLUGIN_VERSION,
        .min_version = RBH_POSIX_BACKEND_VERSION,
        .max_version = RBH_POSIX_BACKEND_VERSION,
        .common_ops  = &RETENTION_EXTENSION_COMMON_OPS,
    },
    .enrich         = rbh_retention_enrich,
    .setup_enricher = rbh_retention_setup,
};
