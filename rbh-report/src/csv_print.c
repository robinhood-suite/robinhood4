/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <robinhood.h>
#include <robinhood/backend.h>
#include <robinhood/uri.h>

#include "report.h"
#include "common_print.h"

static void
dump_id_map(const struct rbh_value_map *map, struct rbh_group_fields group)
{
    char buffer[1024];

    if (map->count != group.id_count)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected number of fields in id map, expected '%ld', got '%ld'",
                      group.id_count, map->count);

    for (int i = 0; i < map->count; i++) {
        dump_value(map->pairs[i].value, buffer);
        printf("%s", buffer);

        if (i < map->count - 1)
            printf(",");
    }
}

static void
dump_output_map(const struct rbh_value_map *map,
                const struct rbh_filter_output output)
{
    char buffer[1024];

    if (map->count != output.output_fields.count)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected number of fields in output map, expected '%ld', got '%ld'",
                      output.output_fields.count, map->count);

    for (int i = 0; i < map->count; i++) {
        dump_value(map->pairs[i].value, buffer);
        printf("%s", buffer);

        if (i < map->count - 1)
            printf(",");
    }
}

void
dump_results(const struct rbh_value_map *result_map,
             const struct rbh_group_fields group,
             const struct rbh_filter_output output)
{
    if (result_map->count == 2) {
        dump_id_map(&result_map->pairs[0].value->map, group);
        printf(": ");
        dump_output_map(&result_map->pairs[1].value->map, output);
    } else {
        dump_output_map(&result_map->pairs[0].value->map, output);
    }
}
