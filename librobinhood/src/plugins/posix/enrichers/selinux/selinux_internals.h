/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_SELINUX_INTERNALS_H
#define ROBINHOOD_SELINUX_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>

#include <robinhood/backend.h>
#include <robinhood/config.h>
#include <robinhood/fsentry.h>
#include <robinhood/action.h>
#include <robinhood/projection.h>

struct entry_info;
struct rbh_value_pair;
struct rbh_sstack;
struct filters_context;

int
rbh_selinux_setup(void);

int
rbh_selinux_enrich(struct entry_info *einfo, uint64_t flags,
                   struct rbh_value_pair *pairs, size_t pairs_count,
                   struct rbh_sstack *values);

enum rbh_parser_token
rbh_selinux_check_valid_token(const char *token);

struct rbh_filter *
rbh_selinux_build_filter(struct filters_context *context, int *index);

enum known_directive
rbh_selinux_fill_entry_info(const struct rbh_fsentry *fsentry,
                           const char *format_string, size_t *index,
                           char *output, size_t *output_length, int max_length,
                           __attribute__((unused)) const char *backend);

enum known_directive
rbh_selinux_fill_projection(struct rbh_filter_projection *projection,
                           const char *format_string, size_t *index);

void
rbh_selinux_helper(__attribute__((unused)) const char *backend,
                  __attribute__((unused)) struct rbh_config *config,
                  char **predicate_helper, char **directive_helper);


#endif
