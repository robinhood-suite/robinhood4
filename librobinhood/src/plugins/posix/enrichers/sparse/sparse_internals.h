/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_SPARSE_INTERNALS_H
#define ROBINHOOD_SPARSE_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>

#include <robinhood/backend.h>
#include <robinhood/config.h>
#include <robinhood/fsentry.h>

struct entry_info;
struct rbh_value_pair;
struct rbh_sstack;

int
rbh_sparse_setup(void);

int
rbh_sparse_enrich(struct entry_info *einfo, uint64_t flags,
                  struct rbh_value_pair *pairs, size_t pairs_count,
                  struct rbh_sstack *values);

#endif
