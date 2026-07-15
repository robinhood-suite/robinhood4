/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "backends/posix.h"
#include "robinhood/backends/acl.h"
#include "robinhood/backends/posix_extension.h"
#include "robinhood/plugins/backend.h"


const struct rbh_posix_extension RBH_BACKEND_EXTENDS(POSIX, ACL) = {
    .extension = {
        .super       = RBH_POSIX_BACKEND_NAME,
        .name        = RBH_ACL_PLUGIN_NAME,
        .version     = RBH_ACL_PLUGIN_VERSION,
        .min_version = RBH_POSIX_BACKEND_VERSION,
        .max_version = RBH_POSIX_BACKEND_VERSION,
        .common_ops  = NULL,
    },
};
