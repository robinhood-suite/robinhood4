/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <sys/stat.h>

#include <robinhood.h>

#include "report.h"

static void
pretty_print_results(const struct rbh_value_map *id_map,
                     const struct rbh_group_fields group,
                     const struct rbh_value_map *output_map,
                     struct result_columns *columns)
{
    const struct rbh_value_map *output_map;
    const struct rbh_value_map *id_map;

    if (result_map->count == 2) {
        id_map = &result_map->pairs[0].value->map;
        output_map = &result_map->pairs[1].value->map;
    } else {
        id_map = NULL;
        output_map = &result_map->pairs[0].value->map;
    }

    check_columns_lengthes(id_map, group, output_map, columns);

    if (pretty_print) {
        pretty_print_results(id_map, group, output_map, columns);
        return;
    }

    if (id_map) {
        dump_id_map(id_map, group);
        printf(": ");
    }

    dump_output_map(output_map, output);
}
