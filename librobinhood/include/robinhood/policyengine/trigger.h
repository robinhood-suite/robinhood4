/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_POLICYENGINE_TRIGGER_H
#define RBH_POLICYENGINE_TRIGGER_H

#include <stdint.h>

#include "robinhood/backend.h"
#include "robinhood/filter.h"

/**
 * Compute a global aggregate over all entries matching @p filter.
 *
 * @param backend     storage backend (e.g. MongoDB)
 * @param filter      pre-built filter to restrict the query; NULL matches all
 * @param stat_field  RBH_STATX_* field to sum (FA_SUM), or 0 to count entries
 *                    (FA_COUNT)
 * @param result      filled with the aggregated value on success; set to 0 if
 *                    no entries match
 *
 * @return            0 on success, -1 with errno set on error
 */
int
rbh_trigger_query_stat(struct rbh_backend *backend,
                       const struct rbh_filter *filter,
                       uint32_t stat_field, int64_t *result);

#endif
