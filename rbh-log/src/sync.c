/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

enum log_value {
    COMMAND_LINE,
    CONVERTED_ENTRIES,
    DURATION,
    END_TIME,
    SKIPPED_ENTRIES,
    SOURCE_MOUNTPOINT,
    START_TIME,
    TOTAL_ENTRIES,
};

static enum log_value
key2log_value(const char *key)
{
    switch (key[0]) {
    case 'c':
        if (key[1] != 'o')
            break;

        if (key[2] == 'm' && !strcmp(&key[3], "mand_line"))
            return COMMAND_LINE;
        else if (key[2] == 'n' && !strcmp(&key[3], "verted_entries"))
            return CONVERTED_ENTRIES;

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
        if (key[1] == 't' && !strcmp(&key[2], "art_time"))
            return START_TIME;
        else if (key[1] == 'o' && !strcmp(&key[2], "urce_mountpoint"))
            return SOURCE_MOUNTPOINT;
        else if (key[1] == 'k' && !strcmp(&key[2], "ipped_entries"))
            return SKIPPED_ENTRIES;

        break;
    case 't':
        if (!strcmp(&key[1], "otal_entries"))
            return TOTAL_ENTRIES;

        break;
    }

    error(EXIT_FAILURE, EINVAL, "Unknown key found in sync log: '%s'", key);
    __builtin_unreachable();
}

static const struct formatted_log_value sync_log_value[] = {
    [START_TIME] =        { .key = "start_time",
                            .header = "Start of the sync",
                            .print_log_value = print_time_from_timestamp },
    [DURATION] =          { .key = "duration",
                            .header = "Duration of the sync",
                            .print_log_value = print_difftime },
    [END_TIME] =          { .key = "end_time",
                            .header = "End of the sync",
                            .print_log_value = print_time_from_timestamp },
    [SOURCE_MOUNTPOINT] = { .key = "source_mountpoint",
                            .header = "Mountpoint used",
                            .print_log_value = print_value },
    [COMMAND_LINE] =      { .key = "command_line",
                            .header = "Command used",
                            .print_log_value = print_value },
    [CONVERTED_ENTRIES] = { .key = "converted_entries",
                            .header = "Amount of entries converted",
                            .print_log_value = print_value },
    [SKIPPED_ENTRIES] =   { .key = "skipped_entries",
                            .header = "Amount of entries skipped",
                            .print_log_value = print_value },
    [TOTAL_ENTRIES] =     { .key = "total_entries",
                            .header = "Total entries seen by the sync",
                            .print_log_value = print_value },
};

void
print_sync_log(const struct rbh_value_map *log, const char *header)
{
    printf("%s:\n", header);

    for (size_t i = 0 ; i < log->count ; i++) {
        const struct rbh_value_pair *pair = &log->pairs[i];
        struct formatted_log_value log_value;

        log_value = sync_log_value[key2log_value(pair->key)];

        log_value.print_log_value(pair->value, log_value.header);
    }
}
