/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef ROBINHOOD_POSIX_EXTENTION_H
#define ROBINHOOD_POSIX_EXTENTION_H

#include <robinhood/plugin.h>

struct rbh_posix_extension {
    struct rbh_plugin_extension extension;
    int (*iterator_new)(void);
    int (*enrich)(void);
};

#endif
