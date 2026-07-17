/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_LOG_H
#define RBH_FIND_LOG_H

#include "core.h"

/**
 * Insert a find log in the backends registered in the given context.
 *
 * @param ctx             the context containing the backends in which to
 *                        insert the log
 * @param metadata        the log to insert
 */
void
insert_find_log(struct find_context *ctx, struct rbh_metadata *metadata);

#endif
