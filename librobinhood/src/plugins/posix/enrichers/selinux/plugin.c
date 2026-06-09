/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "backends/posix.h"
#include "selinux_internals.h"
#include "robinhood/backends/selinux.h"
#include "robinhood/backends/posix_extension.h"
#include "robinhood/plugins/backend.h"

const struct rbh_posix_extension RBH_BACKEND_EXTENDS(POSIX, SELINUX) = {
    .extension = {
        .super       = RBH_POSIX_BACKEND_NAME,
        .name        = RBH_SELINUX_PLUGIN_NAME,
        .version     = RBH_SELINUX_PLUGIN_VERSION,
        .min_version = RBH_POSIX_BACKEND_VERSION,
        .max_version = RBH_POSIX_BACKEND_VERSION,
        .common_ops  = NULL,
    },
    .enrich          = rbh_selinux_enrich,
    .setup_enricher  = rbh_selinux_setup,
};

