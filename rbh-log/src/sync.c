/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

void
print_sync_log(const struct rbh_value_map *log, const char *header)
{
    time_t time;

    printf("%s:\n", header);

    for (size_t i = 0 ; i < log->count ; i++) {
        const struct rbh_value_pair *pair = &log->pairs[i];
        if (strcmp(pair->key, "start_time") == 0) {
            time = (time_t)pair->value->int64;
            printf(" - %-*s %s\n", WIDTH, "Start of the sync:",
                   time_from_timestamp(&time));
        }

        if (strcmp(pair->key, "duration") == 0) {
            char _buffer[32];
            size_t bufsize;
            char *buffer;

            buffer = _buffer;
            bufsize = sizeof(_buffer);

            difftime_printer(buffer, bufsize, pair->value->int64);

            printf(" - %-*s %s\n", WIDTH, "Duration of the sync:",
                   buffer);
        }

        if (strcmp(pair->key, "end_time") == 0) {
            time = (time_t)pair->value->int64;
            printf(" - %-*s %s\n", WIDTH, "End of the sync:",
                   time_from_timestamp(&time));
        }

        if (strcmp(pair->key, "source_mountpoint") == 0)
            printf(" - %-*s %s\n", WIDTH, "Mountpoint used for the sync:",
                   pair->value->string);

        if (strcmp(pair->key, "command_line") == 0)
            printf(" - %-*s %s\n", WIDTH, "Command used for the sync:",
                   pair->value->string);

        if (strcmp(pair->key, "converted_entries") == 0)
            printf(" - %-*s %ld\n", WIDTH, "Amount of entries converted:",
                   pair->value->int64);

        if (strcmp(pair->key, "skipped_entries") == 0)
            printf(" - %-*s %ld\n", WIDTH, "Amount of entries skipped:",
                   pair->value->int64);

        if (strcmp(pair->key, "total_entries") == 0)
            printf(" - %-*s %ld\n", WIDTH,
                   "Total entries seen by the sync:",
                   pair->value->int64);
    }
}
