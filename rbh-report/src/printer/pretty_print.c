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
    char *buffer;

    buffer = RBH_SSTACK_PUSH(values_sstack, NULL, max_length + 1);
    buffer[max_length] = '\0';
    if (field)
        dump_decorated_value(value, field, buffer);
    else
        dump_value(value, buffer);

    printf(" %*s ", max_length, buffer);

    if (rbh_sstack_pop(values_sstack, max_length + 1))
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_pop");

    /* Account for the starting and ending space */
    return max_length + 2;
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
pretty_print_results(struct rbh_list_node *results,
                     const struct rbh_group_fields group,
                     const struct rbh_filter_output output,
                     struct result_columns *columns)
{
    const struct rbh_value_map *output_map = NULL;
    const struct rbh_value_map *id_map = NULL;
    struct rbh_value_map *result_map;
    struct map_node *elem, *tmp;
    char dash_line[4096];
    int line_size;

    (void) output;

    rbh_list_foreach(results, elem, link) {
        result_map = &elem->map;
        if (result_map->count == 2) {
            id_map = &result_map->pairs[0].value->map;
            output_map = &result_map->pairs[1].value->map;
        } else {
            id_map = NULL;
            output_map = &result_map->pairs[0].value->map;
        }

        check_columns_lengths(id_map, group, output_map, columns);
    }

    line_size = pretty_print_headers(columns, id_map != NULL);

    assert(line_size < sizeof(dash_line));

    dash_line[line_size] = '\0';
    memset(dash_line, '-', line_size);

    printf("%s\n", dash_line);

    rbh_list_foreach_safe(results, elem, tmp, link) {
        result_map = &elem->map;
        if (result_map->count == 2) {
            id_map = &result_map->pairs[0].value->map;
            output_map = &result_map->pairs[1].value->map;
        } else {
            id_map = NULL;
            output_map = &result_map->pairs[0].value->map;
        }

        pretty_print_values(id_map, group, output_map, output, columns);
        rbh_list_del(&elem->link);
        free(elem);
    }
}
