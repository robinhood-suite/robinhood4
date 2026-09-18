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

enum output_format {
    OF_NORMAL,
    OF_ONELINE,
};

struct formatted_log_value {
    const char *header;
    void (*print_log_value)(const struct rbh_value *, const char *,
                            enum output_format);
    bool oneline;
};

enum common_log_value {
    CLV_UNKNOWN,
    CLV_COMMAND_LINE,
    CLV_DURATION,
    CLV_END_TIME,
    CLV_START_TIME,
};

void
print_log_wrapper(const struct rbh_value_map *log,
                  enum output_format output_format,
                  const struct formatted_log_value *flv,
                  int (*key2log_value)(const char *));

/**
 * Print a sync log.
 *
 * @param log            the map whose content should be printed
 * @param output_format  how the output should be printed
 */
void
print_sync_log(const struct rbh_value_map *log,
               enum output_format output_format);

/**
 * Print a fsevents log.
 *
 * @param log            the map whose content should be printed
 * @param output_format  how the output should be printed
 */
void
print_fsevents_log(const struct rbh_value_map *log,
                   enum output_format output_format);

/**
 * Print a find log.
 *
 * @param log            the map whose content should be printed
 * @param output_format  how the output should be printed
 */
void
print_find_log(const struct rbh_value_map *log,
               enum output_format output_format);

/**
 * Print a report log.
 *
 * @param log            the map whose content should be printed
 * @param output_format  how the output should be printed
 */
void
print_report_log(const struct rbh_value_map *log,
                 enum output_format output_format);

/**
 * Print a gc log.
 *
 * @param log            the map whose content should be printed
 * @param output_format  how the output should be printed
 */
void
print_gc_log(const struct rbh_value_map *log,
             enum output_format output_format);

/**
 * All following functions are callback for the `print_log_value` field in the
 * `formatted_log_value` structure. They each take in a `rbh_value` to print,
 * a header and how the information should be printed.
 */

/**
 * Expects the value to be a map with 2 pairs containing int64, prints it as a
 * timespec with format "tv_sec.tv_nsec".
 */
void
print_timespec(const struct rbh_value *value, const char *header,
               enum output_format output_format);

/**
 * Expects the value to be int64, prints it as a timestamp.
 */
void
print_time_from_timestamp(const struct rbh_value *value, const char *header,
                          enum output_format output_format);

/**
 * Expects the value to be int64, prints it as string representing a time
 * differential.
 */
void
print_difftime(const struct rbh_value *value, const char *header,
               enum output_format output_format);

/**
 * Print the value as-is, i.e. string as string, int64 as long int, ....
 */
void
print_value(const struct rbh_value *value, const char *header,
            enum output_format output_format);

#endif
