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

#include <robinhood/action.h>
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
void
rbh_sparse_helper(__attribute__((unused)) const char *backend,
                  __attribute__((unused)) struct rbh_config *config,
                  char **predicate_helper, char **directive_helper);

enum rbh_parser_token
rbh_sparse_check_valid_token(const char *token);

struct rbh_filter *
rbh_sparse_build_filter(const char **argv, int argc, int *index,
                        bool *need_prefetch);

int
rbh_sparse_fill_entry_info(char *output, int max_length,
                           const struct rbh_fsentry *fsentry,
                           const char *format_string, size_t *index,
                           const char *backend);

enum known_directive
rbh_sparse_fill_projection(struct rbh_filter_projection *projection,
                           const char *format_string, size_t *index);

#endif
