/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

enum find_log_value {
    ENTRY_COUNT,
    EXEC_SUCCESS_COUNT,
};

static int
key2find_log_value(const char *key)
{
    switch (key[0]) {
    case 'e':
        if (key[1] == 'n' && !strcmp(&key[2], "try_count"))
            return ENTRY_COUNT;

        if (key[1] == 'x' && !strcmp(&key[2], "ec_success_count"))
            return EXEC_SUCCESS_COUNT;

        break;
    }

    error(EXIT_FAILURE, EINVAL, "Unknown key found in find log: '%s'", key);
    __builtin_unreachable();
}

static const struct formatted_log_value find_log_value[] = {
    [ENTRY_COUNT] =
        { .header = "Number of entries post-filtering",
          .oneline_header = "Entries post-filtering",
          .print_log_value = print_value,
          .oneline  = true },
    [EXEC_SUCCESS_COUNT] =
        { .header = "Number of entries which succeeded the exec command",
          .print_log_value = print_value },
};


void
print_find_log(const struct rbh_value_map *log,
               enum output_format output_format)
{
    print_log_wrapper(log, output_format, find_log_value, &key2find_log_value);
}
