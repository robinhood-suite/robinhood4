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

static int
pretty_print_padded_value(int max_length, const struct rbh_value *value)
{
    (void) max_length;
    (void) value;

    return 0;
}

static int
pretty_print_headers(struct result_columns *columns, bool print_id)
{
    int written = 0;

    if (!print_id)
        goto skip_id;

    for (int i = 0; i < columns->id_count; i++) {
        struct rbh_value value = {
            .type = RBH_VT_STRING,
            .string = columns->id_columns[i].header,
        };

        written += pretty_print_padded_value(columns->id_columns[i].length,
                                             &value);

        if (i < columns->id_count - 1)
            written += printf("|");
    }

    written += printf("||");

skip_id:
    for (int i = 0; i < columns->output_count; i++) {
        struct rbh_value value = {
            .type = RBH_VT_STRING,
            .string = columns->output_columns[i].header,
        };

        written += pretty_print_padded_value(columns->output_columns[i].length,
                                             &value);

        if (i < columns->output_count - 1)
            written += printf("|");
    }

    printf("\n");

    return written;
}

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

        check_columns_lengthes(id_map, group, output_map, columns);
    }

    pretty_print_headers(columns, id_map != NULL);
}
