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

static void
dump_value(const struct rbh_value *value)
{
    switch (value->type) {
    case RBH_VT_INT32:
        printf("%d", value->int32);
        break;
    case RBH_VT_INT64:
        printf("%ld", value->int64);
        break;
    case RBH_VT_STRING:
        printf("%s", value->string);
        break;
    case RBH_VT_SEQUENCE:
        printf("[");
        for (int j = 0; j < value->sequence.count; j++) {
            dump_value(&value->sequence.values[j]);
            if (j < value->sequence.count - 1)
                printf("; ");
        }
        printf("]");
        break;
    default:
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected value type, found '%s'",
                      VALUE_TYPE_NAMES[value->type]);
    }
}

void
dump_id_map(const struct rbh_value_map *map, struct rbh_group_fields group)
{
    if (map->count != group.id_count)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected number of fields in id map, expected '%ld', got '%ld'",
                      group.id_count, map->count);

    for (int i = 0; i < map->count; i++) {
        dump_value(map->pairs[i].value);

        if (i < map->count - 1)
            printf(",");
    }
}

void
dump_output_map(const struct rbh_value_map *map,
                const struct rbh_filter_output output)
{
    if (map->count != output.output_fields.count)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected number of fields in output map, expected '%ld', got '%ld'",
                      output.output_fields.count, map->count);

    for (int i = 0; i < map->count; i++) {
        dump_value(map->pairs[i].value);

        if (i < map->count - 1)
            printf(",");
    }
}
