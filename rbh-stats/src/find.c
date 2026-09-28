/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "stats.h"

enum find_stats_value {
    ENTRY_COUNT,
    EXEC_SUCCESS_COUNT,
};

static int
key2find_stats_value(const char *key)
{
    switch (key[0]) {
    case 'e':
        if (key[1] == 'n' && !strcmp(&key[2], "try_count"))
            return ENTRY_COUNT;

        if (key[1] == 'x' && !strcmp(&key[2], "ec_success_count"))
            return EXEC_SUCCESS_COUNT;

        break;
    }

    error(EXIT_FAILURE, EINVAL, "Unknown key found in find stats: '%s'", key);
    __builtin_unreachable();
}

static const struct formatted_stats_value find_stats_value[] = {
    [ENTRY_COUNT] =
        { .header = "Number of entries post-filtering",
          .oneline_header = "Entries post-filtering",
          .print_stats_value = print_value,
          .oneline  = true },
    [EXEC_SUCCESS_COUNT] =
        { .header = "Number of entries which succeeded the exec command",
          .print_stats_value = print_value },
};


void
print_find_stats(const struct rbh_value_map *stats,
                 enum output_format output_format)
{
    print_stats_wrapper(stats, output_format, find_stats_value,
                        &key2find_stats_value);
}
