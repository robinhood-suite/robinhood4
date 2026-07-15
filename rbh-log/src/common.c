/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

void
print_time_from_timestamp(const struct rbh_value *value, const char *header)
{
    time_t time = (time_t) value->int64;

    printf(" - %-*s: %s\n", WIDTH, header, time_from_timestamp(&time));
}

void
print_difftime(const struct rbh_value *value, const char *header)
{
    char _buffer[32];
    size_t bufsize;
    char *buffer;

    buffer = _buffer;
    bufsize = sizeof(_buffer);

    difftime_printer(buffer, bufsize, value->int64);

    printf(" - %-*s: %s\n", WIDTH, header, buffer);
}

void
print_value(const struct rbh_value *value, const char *header)
{
    switch (value->type) {
    case RBH_VT_STRING:
        printf(" - %-*s: %s\n", WIDTH, header, value->string);
        break;
    case RBH_VT_INT64:
        printf(" - %-*s: %ld\n", WIDTH, header, value->int64);
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
    [CLV_START_TIME] =    { .key = "start_time",
                            .header = "Start of the command",
                            .print_log_value = print_time_from_timestamp },
    [CLV_DURATION] =      { .key = "duration",
                            .header = "Duration of the command",
                            .print_log_value = print_difftime },
    [CLV_END_TIME] =      { .key = "end_time",
                            .header = "End of the command",
                            .print_log_value = print_time_from_timestamp },
    [CLV_COMMAND_LINE] =  { .key = "command_line",
                            .header = "Command used",
                            .print_log_value = print_value },
};

void
print_common_log_info(const struct rbh_value *value,
                      enum common_log_value log_value)
{
    struct formatted_log_value formatted_log_value =
        common_formatted_log_value[log_value];

    formatted_log_value.print_log_value(value, formatted_log_value.header);
}
