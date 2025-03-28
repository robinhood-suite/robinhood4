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
 * Build a dynamically allocated options filter
 *
 * @param sorts     a list of sort predicates
 * @param count     the size of \p sorts
 * @param field     a sort attribute
 *
 * @return          a pointer to a newly allocated
 *                  struct rbh_filter_sort
 *
 * Exit on error
 */
struct rbh_filter_sort *
sort_options_append(struct rbh_filter_sort *sorts, size_t count,
                    struct rbh_filter_field field, bool ascending);

#endif
