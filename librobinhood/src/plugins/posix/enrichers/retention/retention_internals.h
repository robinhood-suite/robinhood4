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

#include <robinhood/backend.h>
#include <robinhood/fsentry.h>
#include <robinhood/config.h>

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

/**
 * The following functions are implementations of the different callbacks of the
 * `rbh_pe_common_operations` structure. Their documentation is the same as the
 * one given for the structure's callbacks.
 */
void
rbh_retention_helper(__attribute__((unused)) const char *backend,
                     __attribute__((unused)) struct rbh_config *config,
                     char **predicate_helper, char **directive_helper);

enum rbh_parser_token
rbh_retention_check_valid_token(const char *token);

struct rbh_filter *
rbh_retention_build_filter(const char **argv, int argc, int *index,
                           bool *need_prefetch);

int
rbh_retention_fill_entry_info(char *output, int max_length,
                              const struct rbh_fsentry *fsentry,
                              const char *directive, const char *backend);

/**
 * Fill the projection to retrieve only the information needed
 *
 * @param projection the projection to fill
 * @param directive  which information to retrieve
 *
 * @return           1 on success, 0 if the directive requested is unknown,
 *                   -1 on error
 */
int
rbh_retention_fill_projection(struct rbh_filter_projection *projection,
                              const char *directive);
#endif
