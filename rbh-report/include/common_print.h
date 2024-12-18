/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_REPORT_COMMON_PRINT_H
#define RBH_REPORT_COMMON_PRINT_H

#include <robinhood/value.h>

int
dump_value(const struct rbh_value *value, char *buffer);

int
dump_decorated_value(const struct rbh_value *value,
                     const struct rbh_filter_field *field, char *buffer);

#endif
