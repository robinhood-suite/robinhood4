/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef ROBINHOOD_RETENTION_INTERNALS_H
#define ROBINHOOD_RETENTION_INTERNALS_H

struct entry_info;
struct rbh_value_pair;
struct rbh_sstack;

int
rbh_retention_setup(void);

int
rbh_retention_enrich(struct entry_info *einfo, struct rbh_value_pair *pairs,
                     struct rbh_sstack *values);

#endif
