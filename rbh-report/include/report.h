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

int
fill_acc_and_output_fields(const char *_output_string,
                           struct rbh_group_fields *group,
                           struct rbh_filter_output *output);

void
fill_group_by_fields(const char *_group_by, struct rbh_group_fields *group);

#endif
