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

/**
 * The following functions are implementations of the different callbacks of the
 * `rbh_pe_common_operations` structure. Their documentation is the same as the
 * one given for the structure's callbacks.
 */
enum rbh_parser_token
rbh_sparse_check_valid_token(const char *token);

struct rbh_filter *
rbh_sparse_build_filter(const char **argv, int argc, int *index,
                        bool *need_prefetch);

#endif
