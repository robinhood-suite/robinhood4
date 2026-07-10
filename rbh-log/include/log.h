/* This file is part of RobinHood
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_LOG_H
#define RBH_LOG_H

#include <robinhood/plugin.h>
#include <robinhood/plugins/backend.h>

#define WIDTH 32

/**
 * Print a sync log.
 *
 * @param log       the map whose content corresponds to a sync log and should
 *                  be printed
 * @param header    a string to print as header for the log
 */
void
print_sync_log(const struct rbh_value_map *log, const char *header);

#endif
