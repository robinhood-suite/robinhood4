/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <sys/stat.h>

#include <robinhood.h>

#include "report.h"

void
check_columns_lengths(const struct rbh_value_map *id_map,
                      const struct rbh_value_map *output_map,
                      struct result_columns *columns)
{
    (void) id_map;
    (void) output_map;
    (void) columns;
}

void
init_id_columns(struct result_columns *columns, int id_count)
{
    columns->id_count = id_count;

    if (id_count) {
        columns->id_columns = rbh_sstack_push(
            values_sstack, NULL,
            columns->id_count * sizeof(*(columns->id_columns)));
        if (columns->id_columns == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "rbh_sstack_push");
    } else {
        columns->id_columns = NULL;
    }
}

void
init_output_columns(struct result_columns *columns, int output_count)
{
    columns->output_count = output_count;

    columns->output_columns = rbh_sstack_push(
        values_sstack, NULL,
        columns->output_count * sizeof(*(columns->output_columns)));
    if (columns->output_columns == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_push");
}

void
init_column(struct column *column, const char *string)
{
    column->header = rbh_sstack_push(values_sstack, string, strlen(string) + 1);
    if (column->header == NULL)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__, "strdup");

    column->length = strlen(string);
}
