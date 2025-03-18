/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood/backends/mfu.h>
#include <robinhood/backends/posix_extension.h>
#include <robinhood/backends/posix.h>
#include <robinhood/plugins/backend.h>

#include "mfu_internals.h"

const struct rbh_posix_extension RBH_BACKEND_EXTENDS(POSIX, MFU) = {
    .extension = {
        .super       = RBH_POSIX_BACKEND_NAME,
        .name        = RBH_MFU_PLUGIN_NAME,
        .version     = RBH_MFU_PLUGIN_VERSION,
        .min_version = RBH_POSIX_BACKEND_VERSION,
        .max_version = RBH_POSIX_BACKEND_VERSION,
    },
    .iter_new = rbh_posix_mfu_iter_new,
};
