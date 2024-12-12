/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_REPORT_COLUMNS_H
#define RBH_REPORT_COLUMNS_H

#include <robinhood/value.h>

#include "report.h"

void
check_columns_lengthes(const struct rbh_value_map *id_map,
                       const struct rbh_value_map *output_map,
                       struct result_columns *columns);

void
init_id_columns(struct result_columns *columns, int id_count);

void
init_output_columns(struct result_columns *columns, int output_count);

void
init_column(struct column *column, const char *string);

#endif
