/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_STATS_H
#define RBH_FIND_STATS_H

#include "core.h"

/**
 * Insert a find stats in the backends registered in the given context.
 *
 * @param ctx             the context containing the backends in which to
 *                        insert the stats
 * @param metadata        the stats to insert
 */
void
insert_find_stats(struct find_context *ctx, struct rbh_metadata *metadata);

#endif
