/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <sys/stat.h>

#include <robinhood.h>

#include "columns.h"
#include "report.h"

void
pretty_print_results(struct rbh_value_map *result_maps, int count_results,
                     const struct rbh_group_fields group,
                     const struct rbh_filter_output output,
                     struct result_columns *columns)
{
    const struct rbh_value_map *output_map;
    const struct rbh_value_map *id_map;

    (void) output;

    for (int i = 0; i < count_results; ++i) {
        if (result_maps[i].count == 2) {
            id_map = &result_maps[i].pairs[0].value->map;
            output_map = &result_maps[i].pairs[1].value->map;
        } else {
            id_map = NULL;
            output_map = &result_maps[i].pairs[0].value->map;
        }

        check_columns_lengths(id_map, group, output_map, columns);
    }
}
