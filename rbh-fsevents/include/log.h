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
 * @param start_time       the start timestamp to insert in the log
 * @param end_time         the end timestamp to insert in the log
 * @param command_line     the command line to insert in the log
 * @param sink             the sink in which to insert the log
 */
void
insert_fsevents_log(time_t start_time, time_t end_time, char *command_line,
                    struct sink *sink);

#endif
