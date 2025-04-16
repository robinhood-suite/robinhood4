/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_LUSTRE_INTERNALS_H
#define RBH_LUSTRE_INTERNALS_H

#include <unistd.h>
#include <stdint.h>

#include <robinhood/filter.h>

struct entry_info;
struct rbh_value_pair;
struct rbh_sstack;

int
rbh_lustre_enrich(struct entry_info *einfo, uint64_t flags,
                  struct rbh_value_pair *pairs,
                  size_t pairs_count,
                  struct rbh_sstack *values);

/**
 * The following functions are implementations of the different callbacks of the
 * `rbh_pe_common_operations` structure. Their documentation is the same as the
 * one given for the structure's callbacks.
 */
void
rbh_lustre_helper(__attribute__((unused)) const char *backend,
                  __attribute__((unused)) struct rbh_config *config,
                  char **predicate_helper, char **directive_helper);

enum rbh_parser_token
rbh_lustre_check_valid_token(const char *token);

struct rbh_filter *
rbh_lustre_build_filter(const char **argv, int argc, int *index,
                        bool *need_prefetch);

#endif
