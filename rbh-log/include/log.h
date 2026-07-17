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
    const char *header;
    void (*print_log_value)(const struct rbh_value *, const char *);
};

enum common_log_value {
    CLV_UNKNOWN,
    CLV_COMMAND_LINE,
    CLV_DURATION,
    CLV_END_TIME,
    CLV_START_TIME,
};

/**
 * Convert a key to a common log value.
 *
 * @param key       the key to convert
 *
 * @return          the converted key
 */
enum common_log_value
key2common_log_value(const char *key);

/**
 * Print the given value as if it were common information about a log.
 *
 * Can correspond to the start time, duration, end time and command line.
 *
 * @param value      the value whose content should be printed as common log info
 * @param log_value  the type of information to print
 */
void
print_common_log_info(const struct rbh_value *value,
                      enum common_log_value log_value);

/**
 * Print a sync log.
 *
 * @param log       the map whose content should be printed
 */
void
print_sync_log(const struct rbh_value_map *log);

/**
 * Print a fsevents log.
 *
 * @param log       the map whose content should be printed
 */
void
print_fsevents_log(const struct rbh_value_map *log);

/**
 * Print a find log.
 *
 * @param log       the map whose content should be printed
 */
void
print_find_log(const struct rbh_value_map *log);

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
