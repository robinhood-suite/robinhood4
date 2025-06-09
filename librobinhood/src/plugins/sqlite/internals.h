/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_SQLITE_INTERNALS_H
#define ROBINHOOD_SQLITE_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/backends/sqlite.h>

struct sqlite_backend {
    struct rbh_backend backend;
};

#endif
