/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <sys/stat.h>

#include <robinhood.h>

#include "common_print.h"
#include "columns.h"
#include "report.h"

static int
pretty_print_padded_value(int max_length, struct rbh_filter_field *field,
                          const struct rbh_value *value)
{
    /* Include a starting and ending whitespace */
    int printed_length = max_length + 2;
    char *buffer;
    int done = 0;

    buffer = rbh_sstack_push(values_sstack, NULL, printed_length + 1);
    if (buffer == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_push");

    buffer[printed_length] = '\0';
    buffer[done++] = ' ';
    if (field)
        done += dump_decorated_value(value, field, buffer + done);
    else
        done += dump_value(value, buffer + done);

    for (int i = done; i < printed_length; ++i)
        buffer[i] = ' ';

    printf("%s", buffer);

    if (rbh_sstack_pop(values_sstack, printed_length + 1))
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_pop");

    return printed_length;
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
                                             NULL, &value);

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
                                             NULL, &value);

        if (i < columns->output_count - 1)
            written += printf("|");
    }

    printf("\n");

    return written;
}

static void
pretty_print_values(const struct rbh_value_map *id_map,
                    const struct rbh_group_fields group,
                    const struct rbh_value_map *output_map,
                    const struct rbh_filter_output output,
                    struct result_columns *columns)
{
    if (id_map) {
        for (int i = 0; i < id_map->count; i++) {
            pretty_print_padded_value(columns->id_columns[i].length,
                                      &group.id_fields[i].field,
                                      id_map->pairs[i].value);

            if (i < id_map->count - 1)
                printf("|");
        }

        printf("||");
    }

    for (int i = 0; i < output_map->count; i++) {
        pretty_print_padded_value(columns->output_columns[i].length,
                                  &output.output_fields.fields[i].field,
                                  output_map->pairs[i].value);

        if (i < output_map->count - 1)
            printf("|");
    }

    printf("\n");
}

void
pretty_print_results(struct rbh_value_map *result_maps, int count_results,
                     const struct rbh_group_fields group,
                     const struct rbh_filter_output output,
                     struct result_columns *columns)
{
    const struct rbh_value_map *output_map;
    const struct rbh_value_map *id_map;
    char dash_line[4096];
    int line_size;

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

    line_size = pretty_print_headers(columns, id_map != NULL);

    assert(line_size < sizeof(dash_line));

    dash_line[line_size] = '\0';
    for (int i = 0; i < line_size; i++)
        dash_line[i] = '-';

    printf("%s\n", dash_line);

    for (int i = 0; i < count_results; ++i) {
        if (result_maps[i].count == 2) {
            id_map = &result_maps[i].pairs[0].value->map;
            output_map = &result_maps[i].pairs[1].value->map;
        } else {
            id_map = NULL;
            output_map = &result_maps[i].pairs[0].value->map;
        }

        pretty_print_values(id_map, group, output_map, output, columns);
    }
}
