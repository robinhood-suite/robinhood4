/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_LOG_H
#define RBH_LOG_H

#include <robinhood/plugin.h>
#include <robinhood/plugins/backend.h>

#define WIDTH 32

struct formatted_log_value {
    const char *key;
    const char *header;
    void (*print_log_value)(const struct rbh_value *, const char *);
};

/**
 * Print a sync log.
 *
 * @param log       the map whose content corresponds to a sync log and should
 *                  be printed
 * @param header    a string to print as header for the log
 */
void
print_sync_log(const struct rbh_value_map *log, const char *header);

/**
 * All following functions are callback for the `print_log_value` field in the
 * `formatted_log_value` structure. They each take in a `rbh_value` to print
 * and a header.
 */

/**
 * Expects the value to be int64, prints it as a timestamp.
 */
void
print_time_from_timestamp(const struct rbh_value *value, const char *header);

/**
 * Expects the value to be int64, prints it as string representing a time
 * differential.
 */
void
print_difftime(const struct rbh_value *value, const char *header);

/**
 * Print the value as-is, i.e. string as string, int64 as long int, ....
 */
void
print_value(const struct rbh_value *value, const char *header);

#endif
