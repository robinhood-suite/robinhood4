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
 * Check the given token is a valid Lustre action or predicate
 *
 * @param token     the token to check
 *
 * @return          RBH_TOKEN_ACTION if the token is a Lustre action
 *                  RBH_TOKEN_PREDICATE if the token is a Lustre predicate
 *                  RBH_TOKEN_UNKNOWN if the token is not a Lustre token
 *                  RBH_TOKEN_ERROR otherwise
 */
enum rbh_parser_token
rbh_lustre_check_valid_token(const char *token);

#endif
