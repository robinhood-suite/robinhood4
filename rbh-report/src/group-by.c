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

void
fill_group_by_fields(const char *_group_by, struct rbh_group_fields *group)
{
    const struct rbh_filter_field *filter_field;
    struct rbh_filter_field *fields;
    char *current_field;
    int counter = 0;
    char *group_by;
    int count;

    if (_group_by == NULL) {
        group->id_fields = NULL;
        group->id_count = 0;
        return;
    }

    count = count_fields(_group_by);

    fields = rbh_sstack_push(values_sstack, NULL, count * sizeof(*fields));
    if (fields == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_push");

    group_by = strdup(_group_by);
    if (group_by == NULL)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__, "strdup");

    current_field = strtok(group_by, ",");
    while (current_field) {
        filter_field = str2filter_field(current_field);
        if (filter_field == NULL)
            error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                          "'%s' ill-formed, invalid field", group_by);

        fields[counter++] = *filter_field;
        current_field = strtok(NULL, ",");
    }

    free(group_by);

    group->id_fields = fields;
    group->id_count = count;
}
