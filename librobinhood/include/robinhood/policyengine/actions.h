/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_POLICYENGINE_ACTIONS_H
#define RBH_POLICYENGINE_ACTIONS_H

#include "robinhood/policyengine/core.h"

void
rbh_pe_actions_init(const struct rbh_policy *policy,
                    struct rbh_action_cache *cache);

void
rbh_pe_actions_destroy(struct rbh_action_cache *cache);

struct rbh_action
rbh_pe_select_action(const struct rbh_policy *policy,
                     struct rbh_action_cache *cache,
                     bool has_rule,
                     size_t matched_index);

int
rbh_pe_apply_action(const struct rbh_action *action,
                    struct rbh_fsentry *entry,
                    struct rbh_backend *mi_backend,
                    struct rbh_backend *fs_backend,
                    const struct rbh_pe_common_operations *common_ops);

#endif
