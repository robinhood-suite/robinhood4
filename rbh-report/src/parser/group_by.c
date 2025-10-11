/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <robinhood.h>
#include <robinhood/backend.h>
#include <robinhood/uri.h>

#include "columns.h"
#include "report.h"

static void
set_boundaries(int64_t *boundaries, char *boundaries_string)
{
    char *current_value;
    int counter = 0;
    char *saveptr;

    current_value = strtok_r(boundaries_string, ";", &saveptr);
    while (current_value) {
        if (str2int64_t(current_value, &boundaries[counter++]))
            error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                          "'%s' ill-formed, not a number", current_value);

        current_value = strtok_r(NULL, ";", &saveptr);
    }
}

static void
check_and_set_boundaries(struct rbh_range_field *field, char *field_string)
{
    char *open_bracket = strchr(field_string, '[');
    char *close_bracket;
    int count;

    if (open_bracket == NULL) {
        field->boundaries = NULL;
        field->boundaries_count = 0;
        return;
    }

    close_bracket = strchr(open_bracket, ']');
    if (close_bracket == NULL)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "'%s' ill-formed, missing ']' for boundaries",
                      field_string);

    if (*(close_bracket + 1) != '\0')
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "'%s' ill-formed, additional characters after ']'",
                      field_string);

    *open_bracket = '\0';
    *close_bracket = '\0';

    count = count_char_separated_values(open_bracket + 1, ';');
    if (count == -1)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "'%s' ill-formed, empty boundary or missing value",
                      open_bracket + 1);

    field->boundaries = RBH_SSTACK_PUSH(values_sstack, NULL,
                                        count * sizeof(*field->boundaries));

    set_boundaries(field->boundaries, open_bracket + 1);
    field->boundaries_count = count;
}

void
parse_group_by(const char *_group_by, struct rbh_group_fields *group,
               struct result_columns *columns)
{
    const struct rbh_filter_field *filter_field;
    struct rbh_range_field *fields;
    char *current_field;
    int counter = 0;
    char *group_by;
    char *saveptr;
    int count;

    if (_group_by == NULL) {
        group->id_fields = NULL;
        group->id_count = 0;
        init_id_columns(columns, 0);
        return;
    }

    count = count_char_separated_values(_group_by, ',');
    if (count == -1)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "'%s' ill-formed, empty field", _group_by);

    fields = RBH_SSTACK_PUSH(values_sstack, NULL, count * sizeof(*fields));

    group_by = xstrdup(_group_by);

    init_id_columns(columns, count);

    current_field = strtok_r(group_by, ",", &saveptr);
    while (current_field) {
        /* If there are boundaries to set, this call will modify 'current_field'
         * to set the opening bracket to '\0', so that the 'str2filter_field'
         * only works on the field itself.
         */
        check_and_set_boundaries(&fields[counter], current_field);

        init_column(&columns->id_columns[counter], current_field);

        filter_field = str2filter_field(current_field);
        if (filter_field == NULL)
            error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                          "'%s' ill-formed, invalid field", group_by);

        fields[counter].field = *filter_field;
        counter++;
        current_field = strtok_r(NULL, ",", &saveptr);
    }

    free(group_by);

    group->id_fields = fields;
    group->id_count = count;
}
