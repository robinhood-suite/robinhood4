/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "backends/posix.h"
#include "retention_internals.h"
#include "robinhood/backends/retention.h"
#include "robinhood/backends/posix_extension.h"
#include "robinhood/plugins/backend.h"

const struct rbh_posix_extension RBH_BACKEND_EXTENDS(POSIX, RETENTION) = {
    .extension = {
        .super = RBH_POSIX_BACKEND_NAME,
        .name = RBH_RETENTION_PLUGIN_NAME,
        .version = RBH_RETENTION_PLUGIN_VERSION,
        .min_version = RBH_POSIX_BACKEND_VERSION,
        .max_version = RBH_POSIX_BACKEND_VERSION,
    },
    .enrich = rbh_retention_enrich,
};
