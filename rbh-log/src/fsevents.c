/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

enum fsevents_log_value {
    ENRICH_MOUNTPOINT,
    SOURCE_READ,
    WORKER_COUNT,
};

static enum fsevents_log_value
key2fsevents_log_value(const char *key)
{
    switch (key[0]) {
    case 'e':
        if (!strcmp(&key[1], "nrich_mountpoint"))
            return ENRICH_MOUNTPOINT;

        break;
    case 's':
        if (!strcmp(&key[1], "ource_read"))
            return SOURCE_READ;

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
    [ENRICH_MOUNTPOINT] = { .header = "Enrichment mountpoint",
                            .print_log_value = print_value },
    [SOURCE_READ] =       { .header = "Source of the events",
                            .print_log_value = print_value },
    [WORKER_COUNT] =      { .header = "Number of parallel workers used",
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
