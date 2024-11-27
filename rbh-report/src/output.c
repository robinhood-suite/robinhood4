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

#include "report.h"

int
count_fields(const char *str)
{
    /* Even if the string has no comma, it still has at least one field */
    int count = 1;

    while (*str) {
        if (*str == ',')
            count++;

        str++;
    }

    return count;
}

static enum field_accumulator
str2accumulator(const char *str)
{
    switch (*str++) {
    case 'a': /* avg */
        if (strcmp(str, "vg"))
            break;

        return FA_AVG;
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
convert_output_string_to_accumulator_field(char *output_string)
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
        opening = output_string;
        field.accumulator = FA_NONE;
    }

    filter_field = str2filter_field(opening + 1);
    if (filter_field == NULL)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "'%s' ill-formed, invalid field", output_string);

    field.field = *filter_field;

    return field;
}

int
fill_acc_and_output_fields(const char *_output_string,
                           struct rbh_group_fields *group,
                           struct rbh_filter_output *output)
{
    struct rbh_accumulator_field *fields;
    char *output_string;
    int count;

    count = count_fields(_output_string);

    fields = rbh_sstack_push(values_sstack, NULL, count * sizeof(*fields));
    if (fields == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_push");

    output_string = strdup(_output_string);
    if (output_string == NULL)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__, "strdup");

    if (count == 1) {
        fields[0] = convert_output_string_to_accumulator_field(output_string);
    } else {
        char *current_field;
        int counter = 0;
        char *token;

        current_field = output_string;
        token = strchr(current_field, ',');
        while (token) {
            *token = '\0';
            fields[counter++] =
                convert_output_string_to_accumulator_field(current_field);
            current_field = token + 1;
            token = strchr(current_field, ',');
        }

        fields[counter] =
            convert_output_string_to_accumulator_field(current_field);
    }

    free(output_string);

    group->acc_fields = fields;
    group->acc_count = count;
    output->type = RBH_FOT_VALUES;
    output->output_fields.fields = fields;
    output->output_fields.count = count;

    return count;
}
