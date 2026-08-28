/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

void
print_timespec(const struct rbh_value *value, const char *header,
               bool print_oneline)
{
    struct timespec timespec;

    assert(value->type == RBH_VT_MAP);
    assert(value->map.count == 2);

    timespec.tv_sec = value->map.pairs[0].value->int64;
    timespec.tv_nsec = value->map.pairs[1].value->int64;

    if (print_oneline)
        printf("%s: %lu.%09lu", header,
               timespec.tv_sec, timespec.tv_nsec);
    else
        printf(" - %-*s: %lu.%09lu\n", WIDTH, header,
               timespec.tv_sec, timespec.tv_nsec);
}

void
print_time_from_timestamp(const struct rbh_value *value, const char *header,
                          bool print_oneline)
{
    time_t time = (time_t) value->int64;

    if (print_oneline)
        printf("%s: %s", header, time_from_timestamp(&time));
    else
        printf(" - %-*s: %s\n", WIDTH, header, time_from_timestamp(&time));
}

void
print_difftime(const struct rbh_value *value, const char *header,
               bool print_oneline)
{
    char _buffer[32];
    size_t bufsize;
    char *buffer;

    buffer = _buffer;
    bufsize = sizeof(_buffer);

    difftime_printer(buffer, bufsize, value->int64);

    if (print_oneline)
        printf("%s: %s", header, buffer);
    else
        printf(" - %-*s: %s\n", WIDTH, header, buffer);
}

void
print_value(const struct rbh_value *value, const char *header,
            bool print_oneline)
{
    switch (value->type) {
    case RBH_VT_STRING:
        if (print_oneline)
            printf("%s: %s", header, value->string);
        else
            printf(" - %-*s: %s\n", WIDTH, header, value->string);
        break;
    case RBH_VT_INT64:
        if (print_oneline)
            printf("%s: %ld", header, value->int64);
        else
            printf(" - %-*s: %ld\n", WIDTH, header, value->int64);
        break;
    case RBH_VT_DOUBLE:
        if (print_oneline)
            printf("%s: %.3f", header,
                   ((int) (10000 * value->float64)) / 10000.0);
        else
            printf(" - %-*s: %.3f\n", WIDTH, header,
                   ((int) (10000 * value->float64)) / 10000.0);
        break;
    default:
        error(EXIT_FAILURE, EINVAL, "Unexpected key type to print '%s': %d",
              header, value->type);
        __builtin_unreachable();
    }
}

enum common_log_value
key2common_log_value(const char *key)
{
    switch (key[0]) {
    case 'c':
        if (!strcmp(&key[1], "ommand_line"))
            return CLV_COMMAND_LINE;

        break;
    case 'd':
        if (!strcmp(&key[1], "uration"))
            return CLV_DURATION;

        break;
    case 'e':
        if (!strcmp(&key[1], "nd_time"))
            return CLV_END_TIME;

        break;
    case 's':
        if (!strcmp(&key[1], "tart_time"))
            return CLV_START_TIME;

        break;
    }

    return CLV_UNKNOWN;
}

static const struct formatted_log_value common_formatted_log_value[] = {
    [CLV_START_TIME] =    { .header = "Start of the command",
                            .print_log_value = print_time_from_timestamp,
                            .oneline = true },
    [CLV_DURATION] =      { .header = "Duration of the command",
                            .print_log_value = print_difftime,
                            .oneline = true },
    [CLV_END_TIME] =      { .header = "End of the command",
                            .print_log_value = print_time_from_timestamp },
    [CLV_COMMAND_LINE] =  { .header = "Command used",
                            .print_log_value = print_value },
};

void
print_common_log_info(const struct rbh_value *value,
                      enum common_log_value log_value,
                      bool print_oneline,
                      bool *need_comma)
{
    struct formatted_log_value formatted_log_value =
        common_formatted_log_value[log_value];

    if (print_oneline && formatted_log_value.oneline) {
        if (*need_comma)
            printf(", ");

        formatted_log_value.print_log_value(value, formatted_log_value.header,
                                            print_oneline);
        *need_comma = true;
    } else if (!print_oneline) {
        formatted_log_value.print_log_value(value, formatted_log_value.header,
                                            print_oneline);
    }
}
