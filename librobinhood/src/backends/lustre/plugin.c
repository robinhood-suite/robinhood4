/* This file is part of RobinHood 4
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
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

const struct rbh_posix_extension RBH_BACKEND_EXTENDS(POSIX, LUSTRE) = {
    .extension = {
        .super       = RBH_POSIX_BACKEND_NAME,
        .name        = RBH_LUSTRE_BACKEND_NAME,
        .version     = RBH_LUSTRE_BACKEND_VERSION,
        .min_version = RBH_POSIX_BACKEND_VERSION,
        .max_version = RBH_POSIX_BACKEND_VERSION,
    },
    .enrich = rbh_lustre_enrich,
};
