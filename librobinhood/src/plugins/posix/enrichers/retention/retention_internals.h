/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_RETENTION_INTERNALS_H
#define ROBINHOOD_RETENTION_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>

#include <robinhood/fsentry.h>

struct entry_info;
struct rbh_value_pair;
struct rbh_sstack;

int
rbh_retention_setup(void);

int
rbh_retention_enrich(struct entry_info *einfo, uint64_t flags,
                     struct rbh_value_pair *pairs,
                     size_t pairs_count,
                     struct rbh_sstack *values);

struct rbh_filter *
rbh_retention_build_filter(const char **argv, int argc, int *index,
                           bool *need_prefetch);

int
rbh_retention_fill_entry_info(char *output, int max_length,
                              const struct rbh_fsentry *fsentry,
                              const char *directive, const char *backend);

#endif
