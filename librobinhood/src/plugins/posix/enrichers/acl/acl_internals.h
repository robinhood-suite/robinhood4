/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_ACL_INTERNALS_H
#define ROBINHOOD_ACL_INTERNALS_H

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
rbh_acl_setup(void);

int
rbh_acl_enrich(struct entry_info *einfo, uint64_t flags,
               struct rbh_value_pair *pairs, size_t pairs_count,
               struct rbh_sstack *values);

enum rbh_parser_token
rbh_acl_check_valid_token(const char *token);

struct rbh_filter *
rbh_acl_build_filter(struct filters_context *context, int *index);
#endif
