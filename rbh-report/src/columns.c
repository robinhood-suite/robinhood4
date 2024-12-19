/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <sys/stat.h>

#include <robinhood.h>

#include "common_print.h"
#include "report.h"

void
check_columns_lengths(const struct rbh_value_map *id_map,
                      const struct rbh_group_fields group,
                      const struct rbh_value_map *output_map,
                      struct result_columns *columns)
{
    char buffer[1024];

    if (id_map) {
        for (int i = 0; i < id_map->count; i++) {
            int length = dump_decorated_value(id_map->pairs[i].value,
                                              &group.id_fields[i].field,
                                              buffer);

            if (length > columns->id_columns[i].length)
                columns->id_columns[i].length = length;
        }
    }

    for (int i = 0; i < output_map->count; i++) {
        int length = dump_value(output_map->pairs[i].value, buffer);

        if (length > columns->output_columns[i].length)
            columns->output_columns[i].length = length;
    }
}

void
init_id_columns(struct result_columns *columns, int id_count)
{
    columns->id_count = id_count;

    if (id_count)
        columns->id_columns = RBH_SSTACK_PUSH(
            values_sstack, NULL,
            columns->id_count * sizeof(*(columns->id_columns)));
    else
        columns->id_columns = NULL;
}

void
init_output_columns(struct result_columns *columns, int output_count)
{
    columns->output_count = output_count;

    columns->output_columns = RBH_SSTACK_PUSH(
        values_sstack, NULL,
        columns->output_count * sizeof(*(columns->output_columns)));
}

void
init_column(struct column *column, const char *string)
{
    column->header = RBH_SSTACK_PUSH(values_sstack, string, strlen(string) + 1);
    column->length = strlen(string);
}
