/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_LDISKFS_INTERNALS_H
#define ROBINHOOD_LDISKFS_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/backends/ldiskfs.h>

struct ldiskfs_backend {
    struct rbh_backend backend;
};

void
ldiskfs_backend_destroy(void *backend);

#endif
