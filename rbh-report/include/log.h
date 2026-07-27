/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_REPORT_LOG_H
#define RBH_REPORT_LOG_H

#include <robinhood/backend.h>
#include <robinhood/log.h>

/**
 * Insert a report log in the backend.
 *
 * @param backend         the backend in which to insert the log
 * @param metadata        the log to insert
 */
void
insert_report_log(struct rbh_backend *backend, struct rbh_metadata *metadata);

#endif
