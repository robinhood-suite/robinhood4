/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

enum fsevents_log_value {
    CHANGELOG_READ,
    ENRICH_MOUNTPOINT,
    SOURCE_READ,
    START_INDEX,
    TIME_READ_DEDUP,
    TIME_ENRICH_UPDATE,
    WORKER_COUNT,
};

static enum fsevents_log_value
key2fsevents_log_value(const char *key)
{
    switch (key[0]) {
    case 'c':
        if (!strcmp(&key[1], "hangelog_read"))
            return CHANGELOG_READ;

        break;
    case 'e':
        if (!strcmp(&key[1], "nrich_mountpoint"))
            return ENRICH_MOUNTPOINT;

        break;
    case 's':
        if (key[1] == 'o' && !strcmp(&key[2], "urce_read"))
            return SOURCE_READ;

        if (key[1] == 't' && !strcmp(&key[2], "art_index"))
            return START_INDEX;

        break;
    case 't':
        if (strncmp(&key[1], "ime_", 4))
            break;

        if (key[5] == 'r' && !strcmp(&key[6], "ead_dedup"))
            return TIME_READ_DEDUP;

        if (key[5] == 'e' && !strcmp(&key[6], "nrich_update"))
            return TIME_ENRICH_UPDATE;

        break;
    case 'w':
        if (!strcmp(&key[1], "orker_count"))
            return WORKER_COUNT;

        break;
    }

    error(EXIT_FAILURE, EINVAL, "Unknown key found in fsevents log: '%s'", key);
    __builtin_unreachable();
}

static const struct formatted_log_value fsevents_log_value[] = {
    [CHANGELOG_READ] =     { .header = "Amount of changelog read",
                             .print_log_value = print_value },
    [ENRICH_MOUNTPOINT] =  { .header = "Enrichment mountpoint",
                             .print_log_value = print_value },
    [SOURCE_READ] =        { .header = "Source of the events",
                             .print_log_value = print_value },
    [START_INDEX] =        { .header = "Starting index for reading changelogs",
                             .print_log_value = print_value },
    [TIME_READ_DEDUP] =    { .header = "Time spent reading/deduplicating events",
                             .print_log_value = print_timespec },
    [TIME_ENRICH_UPDATE] = { .header = "Time spent enriching/updating mirror (on average between all workers)",
                             .print_log_value = print_timespec },
    [WORKER_COUNT] =       { .header = "Number of parallel workers used",
                             .print_log_value = print_value },
};

void
print_fsevents_log(const struct rbh_value_map *log)
{
    for (size_t i = 0 ; i < log->count ; i++) {
        const struct rbh_value_pair *pair = &log->pairs[i];
        enum common_log_value common_log_value;
        struct formatted_log_value log_value;

        common_log_value = key2common_log_value(pair->key);
        if (common_log_value != CLV_UNKNOWN) {
            print_common_log_info(pair->value, common_log_value);
            continue;
        }

        log_value = fsevents_log_value[key2fsevents_log_value(pair->key)];

        log_value.print_log_value(pair->value, log_value.header);
    }
}
