/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

enum gc_log_value {
    CHECK_COMMAND,
    DELETED_ENTRIES,
    NOT_DELETED_ENTRIES,
    SYNC_TIME,
    TOTAL_ENTRIES,
};

static enum gc_log_value
key2gc_log_value(const char *key)
{
    switch (key[0]) {
    case 'c':
        if (!strcmp(&key[1], "heck_command"))
            return CHECK_COMMAND;

        break;
    case 'd':
        if (!strcmp(&key[1], "eleted_entries"))
            return DELETED_ENTRIES;

        break;
    case 'n':
        if (!strcmp(&key[1], "ot_deleted_entries"))
            return NOT_DELETED_ENTRIES;

        break;
    case 's':
        if (!strcmp(&key[1], "ync_time"))
            return SYNC_TIME;

        break;
    case 't':
        if (!strcmp(&key[1], "otal_entries"))
            return TOTAL_ENTRIES;

        break;
    }

    error(EXIT_FAILURE, EINVAL, "Unknown key found in gc log: '%s'", key);
    __builtin_unreachable();
}

static const struct formatted_log_value gc_formatted_log_value[] = {
    [CHECK_COMMAND] =       { .header = "Check command used",
                              .print_log_value = print_value },
    [DELETED_ENTRIES] =     { .header = "Amount of deleted entries",
                              .print_log_value = print_value },
    [NOT_DELETED_ENTRIES] = { .header = "Amount of non-deleted entries",
                              .print_log_value = print_value },
    [SYNC_TIME] =           { .header = "Sync-time used",
                              .print_log_value = print_value },
    [TOTAL_ENTRIES] =       { .header = "Amount of entries seen",
                              .print_log_value = print_value },
};

void
print_gc_log(const struct rbh_value_map *log)
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

        log_value = gc_formatted_log_value[key2gc_log_value(pair->key)];

        log_value.print_log_value(pair->value, log_value.header);
    }
}
