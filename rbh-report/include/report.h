/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_REPORT_H
#define RBH_REPORT_H

#include <robinhood/backend.h>

extern struct rbh_sstack *values_sstack;

struct column {
    char *header;
    size_t length;
};

struct result_columns {
    struct column *id_columns;
    int id_count;

    struct column *output_columns;
    int output_count;
};

int
count_fields(const char *str);

void
fill_acc_and_output_fields(const char *_output_string,
                           struct rbh_group_fields *group,
                           struct rbh_filter_output *output,
                           struct result_columns *columns);

void
fill_group_by_fields(const char *_group_by, struct rbh_group_fields *group,
                     struct result_columns *columns);

void
dump_results(const struct rbh_value_map *result_map,
             const struct rbh_group_fields group,
             const struct rbh_filter_output output);

void
pretty_print_results(struct rbh_value_map *result_maps, int count_results,
                     const struct rbh_group_fields group,
                     const struct rbh_filter_output output,
                     struct result_columns *columns);

#endif
