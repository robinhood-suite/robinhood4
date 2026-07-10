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

void
print_sync_log(const struct rbh_value_map *log, const char *header)
{
    time_t time;

    printf("%s:\n", header);

    for (size_t i = 0 ; i < log->count ; i++) {
        const struct rbh_value_pair *pair = &log->pairs[i];
        enum log_value log_value = key2log_value(pair->key);

        switch (log_value) {
        case START_TIME:
            time = (time_t)pair->value->int64;
            printf(" - %-*s %s\n", WIDTH, "Start of the sync:",
                   time_from_timestamp(&time));
            break;
        case DURATION: {
            char _buffer[32];
            size_t bufsize;
            char *buffer;

            buffer = _buffer;
            bufsize = sizeof(_buffer);

            difftime_printer(buffer, bufsize, pair->value->int64);

            printf(" - %-*s %s\n", WIDTH, "Duration of the sync:",
                   buffer);
            break;
        }
        case END_TIME:
            time = (time_t)pair->value->int64;
            printf(" - %-*s %s\n", WIDTH, "End of the sync:",
                   time_from_timestamp(&time));
            break;
        case SOURCE_MOUNTPOINT:
            printf(" - %-*s %s\n", WIDTH, "Mountpoint used for the sync:",
                   pair->value->string);
            break;
        case COMMAND_LINE:
            printf(" - %-*s %s\n", WIDTH, "Command used for the sync:",
                   pair->value->string);
            break;
        case CONVERTED_ENTRIES:
            printf(" - %-*s %ld\n", WIDTH, "Amount of entries converted:",
                   pair->value->int64);
            break;
        case SKIPPED_ENTRIES:
            printf(" - %-*s %ld\n", WIDTH, "Amount of entries skipped:",
                   pair->value->int64);
            break;
        case TOTAL_ENTRIES:
            printf(" - %-*s %ld\n", WIDTH,
                   "Total entries seen by the sync:",
                   pair->value->int64);
            break;
        default:
            __builtin_unreachable();
        }
    }
}
