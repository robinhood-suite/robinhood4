/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
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

static enum field_accumulator
str2accumulator(const char *str)
{
    switch (*str++) {
    case 'a': /* avg */
        if (strcmp(str, "vg"))
            break;

        return FA_AVG;
    case 'c': /* count */
        if (strcmp(str, "ount"))
            break;

        return FA_COUNT;
    case 'm':
        switch (*str++) {
        case 'a': /* max */
            if (strcmp(str, "x"))
                break;

            return FA_MAX;
        case 'i': /* min */
            if (strcmp(str, "n"))
                break;

            return FA_MIN;
        }

        break;
    case 's': /* sum */
        if (strcmp(str, "um"))
            break;

        return FA_SUM;
    }

    error(EX_USAGE, 0, "invalid accumulator '%s'", str);
    __builtin_unreachable();
}

static const struct rbh_accumulator_field
convert_string_to_accumulator_field(char *output_string)
{
    const struct rbh_filter_field *filter_field;
    struct rbh_accumulator_field field;
    char *opening;

    if (strlen(output_string) == 0)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "empty field given");

    opening = strchr(output_string, '(');
    if (opening) {
        char *closing = strchr(output_string, ')');

        if (closing == NULL)
            error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                          "'%s' ill-formed, missing ')'", output_string);

        *opening = '\0';
        *closing = '\0';
        field.accumulator = str2accumulator(output_string);
    } else {
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "'%s' ill-formed, should be \"<accumulator>(<field>)\"",
                      output_string);
    }

    if (field.accumulator == FA_COUNT)
        return field;

    filter_field = str2filter_field(opening + 1);
    if (filter_field == NULL)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "'%s' ill-formed, invalid field", output_string);

    field.field = *filter_field;

    return field;
}

void
parse_output(const char *_output_string, struct rbh_group_fields *group,
             struct rbh_filter_output *output, struct result_columns *columns)
{
    struct rbh_accumulator_field *fields;
    char *current_field;
    char *output_string;
    int counter = 0;
    int count;

    count = count_char_separated_values(_output_string, ',');
    if (count == -1)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "'%s' ill-formed, empty field", _output_string);

    fields = RBH_SSTACK_PUSH(values_sstack, NULL, count * sizeof(*fields));

    output_string = strdup(_output_string);
    if (output_string == NULL)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__, "strdup");

    init_output_columns(columns, count);

    current_field = strtok(output_string, ",");
    while (current_field) {
        init_column(&columns->output_columns[counter], current_field);

        fields[counter++] =
            convert_string_to_accumulator_field(current_field);

        current_field = strtok(NULL, ",");
    }

    free(output_string);

    group->acc_fields = fields;
    group->acc_count = count;
    output->type = RBH_FOT_VALUES;
    output->output_fields.fields = fields;
    output->output_fields.count = count;
}
