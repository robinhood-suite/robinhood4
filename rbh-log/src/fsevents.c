/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

enum log_value {
    COMMAND_LINE,
    DURATION,
    END_TIME,
    START_TIME,
};

static enum log_value
key2log_value(const char *key)
{
    switch (key[0]) {
    case 'c':
        if (!strcmp(&key[1], "ommand_line"))
            return COMMAND_LINE;

        break;
    case 'd':
        if (!strcmp(&key[1], "uration"))
            return DURATION;

        break;
    case 'e':
        if (!strcmp(&key[1], "nd_time"))
            return END_TIME;

        break;
    case 's':
        if (!strcmp(&key[1], "tart_time"))
            return START_TIME;

        break;
    }

    error(EXIT_FAILURE, EINVAL, "Unknown key found in fsevents log: '%s'", key);
    __builtin_unreachable();
}

static const struct formatted_log_value fsevents_log_value[] = {
    [START_TIME] =        { .key = "start_time",
                            .header = "Start of fsevents",
                            .print_log_value = print_time_from_timestamp },
    [DURATION] =          { .key = "duration",
                            .header = "Duration of fsevents",
                            .print_log_value = print_difftime },
    [END_TIME] =          { .key = "end_time",
                            .header = "End of fsevents",
                            .print_log_value = print_time_from_timestamp },
    [COMMAND_LINE] =      { .key = "command_line",
                            .header = "Command used",
                            .print_log_value = print_value },
};

void
print_fsevents_log(const struct rbh_value_map *log)
{
    printf("Last fsevents:\n");

    for (size_t i = 0 ; i < log->count ; i++) {
        const struct rbh_value_pair *pair = &log->pairs[i];
        struct formatted_log_value log_value;

        log_value = fsevents_log_value[key2log_value(pair->key)];

        log_value.print_log_value(pair->value, log_value.header);
    }
}
