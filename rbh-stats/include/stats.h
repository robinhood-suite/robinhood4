/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_STATS_H
#define RBH_STATS_H

#include <robinhood/plugin.h>
#include <robinhood/plugins/backend.h>

struct output_handler;

#define WIDTH 32

enum output_format {
    OF_NORMAL,
    OF_ONELINE,
    OF_CSV,
    OF_JSON,
};

struct formatted_stats_value {
    const char *header;
    const char *oneline_header;
    void (*print_stats_value)(const struct rbh_value *,
                              struct output_handler *);
    bool oneline;
};

enum common_stats_value {
    CSV_UNKNOWN,
    CSV_COMMAND_LINE,
    CSV_DURATION,
    CSV_END_TIME,
    CSV_START_TIME,
};

void
print_stats_wrapper(const struct rbh_value_map *stats,
                    enum output_format output_format,
                    const struct formatted_stats_value *flv,
                    int (*key2stats_value)(const char *));

/**
 * Print a sync stats.
 *
 * @param stats            the map whose content should be printed
 * @param output_format    how the output should be printed
 */
void
print_sync_stats(const struct rbh_value_map *stats,
                 enum output_format output_format);

/**
 * Print a fsevents stats.
 *
 * @param stats            the map whose content should be printed
 * @param output_format    how the output should be printed
 */
void
print_fsevents_stats(const struct rbh_value_map *stats,
                     enum output_format output_format);

/**
 * Print a find stats.
 *
 * @param stats            the map whose content should be printed
 * @param output_format    how the output should be printed
 */
void
print_find_stats(const struct rbh_value_map *stats,
                 enum output_format output_format);

/**
 * Print a report stats.
 *
 * @param stats            the map whose content should be printed
 * @param output_format    how the output should be printed
 */
void
print_report_stats(const struct rbh_value_map *stats,
                   enum output_format output_format);

/**
 * Print a gc stats.
 *
 * @param stats            the map whose content should be printed
 * @param output_format    how the output should be printed
 */
void
print_gc_stats(const struct rbh_value_map *stats,
               enum output_format output_format);

/**
 * All following functions are callback for the `print_stats_value` field in the
 * `formatted_stats_value` structure. They each take in a `rbh_value` to print,
 * a header and how the information should be printed.
 */

/**
 * Expects the value to be a map with 2 pairs containing int64, prints it as a
 * timespec with format "tv_sec.tv_nsec".
 */
void
print_timespec(const struct rbh_value *value, struct output_handler *oh);

/**
 * Expects the value to be int64, prints it as a timestamp.
 */
void
print_time_from_timestamp(const struct rbh_value *value,
                          struct output_handler *oh);

/**
 * Expects the value to be int64, prints it as string representing a time
 * differential.
 */
void
print_difftime(const struct rbh_value *value, struct output_handler *oh);

/**
 * Print the value as-is, i.e. string as string, int64 as long int, ....
 */
void
print_value(const struct rbh_value *value, struct output_handler *oh);

#endif
