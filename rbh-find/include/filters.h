/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_FILTERS_H
#define RBH_FIND_FILTERS_H

#include "parser.h"
#include "utils.h"

#include <robinhood/filter.h>

/**
 * Build a filter field from the -sort/-rsort attribute
 *
 * @param attribute    a string representing an attribute
 *
 * @return             a filter field
 *
 * Exit on Error
 */
struct rbh_filter_field
str2field(const char *attribute);

/**
 * Add to the list of sort options in \p options a new sort option defined
 * with \p field and \p ascending
 *
 * @param options    the list of filter options
 * @param field      a sort attribute
 * @param ascending  whether the sort is in ascending order or not
 *
 * Exit on error
 */
void
sort_options_append(struct rbh_filter_options *options,
                    struct rbh_filter_field field, bool ascending);

#endif
