/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/backends/posix.h>
#include <robinhood/backends/posix_extension.h>
#include <robinhood/backends/lustre.h>

#include "lustre_internals.h"

static const struct rbh_pe_common_operations LUSTRE_EXTENSION_COMMON_OPS = {
    .helper = rbh_lustre_helper,
    .check_valid_token = rbh_lustre_check_valid_token,
    .build_filter = rbh_lustre_build_filter,
    .delete_entry = NULL,
};

const struct rbh_posix_extension RBH_BACKEND_EXTENDS(POSIX, LUSTRE) = {
    .extension = {
        .super       = RBH_POSIX_BACKEND_NAME,
        .name        = RBH_LUSTRE_BACKEND_NAME,
        .version     = RBH_LUSTRE_BACKEND_VERSION,
        .min_version = RBH_POSIX_BACKEND_VERSION,
        .max_version = RBH_POSIX_BACKEND_VERSION,
        .common_ops  = &LUSTRE_EXTENSION_COMMON_OPS,
    },
    .enrich = rbh_lustre_enrich,
};
