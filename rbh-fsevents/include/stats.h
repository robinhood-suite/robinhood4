/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_FSEVENTS_STATS_H
#define RBH_FSEVENTS_STATS_H

#include "sink.h"

/**
 * Insert a fsevents stats in the given sink.
 *
 * @param sink             the sink in which to insert the stats
 * @param metadata         the stats to insert
 */
void
insert_fsevents_stats(struct sink *sink, struct rbh_metadata *metadata);

#endif
