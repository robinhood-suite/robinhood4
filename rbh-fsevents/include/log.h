/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_FSEVENTS_LOG_H
#define RBH_FSEVENTS_LOG_H

#include "sink.h"

/**
 * Insert a fsevents log in the given sink.
 *
 * @param sink             the sink in which to insert the log
 * @param metadata         the log to insert
 */
void
insert_fsevents_log(struct sink *sink, struct rbh_metadata *metadata);

#endif
