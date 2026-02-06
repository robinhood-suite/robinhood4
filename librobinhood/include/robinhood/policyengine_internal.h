/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_POLICYENGINE_INTERNAL_H
#define RBH_POLICYENGINE_INTERNAL_H

#include "robinhood/policyengine.h"
#include "robinhood/value.h"

bool
compare_values(enum rbh_filter_operator op,
               const struct rbh_value *field_val,
               const struct rbh_value *filter_val);

bool
rbh_filter_matches_fsentry(const struct rbh_filter *filter,
                           const struct rbh_fsentry *fsentry);

#endif
