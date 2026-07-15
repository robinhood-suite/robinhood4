/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

enum sync_log_value {
    CONVERTED_ENTRIES,
    SKIPPED_ENTRIES,
    SOURCE_MOUNTPOINT,
    TOTAL_ENTRIES,
};

static enum sync_log_value
key2log_value(const char *key)
{
    switch (key[0]) {
    case 'c':
        if (!strcmp(&key[1], "onverted_entries"))
            return CONVERTED_ENTRIES;

        break;
    case 's':
        if (key[1] == 'o' && !strcmp(&key[2], "urce_mountpoint"))
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

static const struct formatted_log_value sync_formatted_log_value[] = {
    [SOURCE_MOUNTPOINT] = { .key = "source_mountpoint",
                            .header = "Mountpoint used",
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
print_sync_log(const struct rbh_value_map *log)
{
    for (size_t i = 0 ; i < log->count ; i++) {
        const struct rbh_value_pair *pair = &log->pairs[i];
        struct formatted_log_value log_value;
        enum common_log_value common_log_value;

        common_log_value = key2common_log_value(pair->key);
        if (common_log_value != CLV_UNKNOWN) {
            print_common_log_info(pair->value, common_log_value);
            continue;
        }

        log_value = sync_formatted_log_value[key2log_value(pair->key)];

        log_value.print_log_value(pair->value, log_value.header);
    }
}
