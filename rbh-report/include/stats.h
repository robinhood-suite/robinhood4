/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_REPORT_STATS_H
#define RBH_REPORT_STATS_H

#include <robinhood/backend.h>
#include <robinhood/stats.h>

/**
 * Insert a report stats in the backend.
 *
 * @param backend         the backend in which to insert the stats
 * @param metadata        the stats to insert
 */
void
insert_report_stats(struct rbh_backend *backend, struct rbh_metadata *metadata);

#endif
