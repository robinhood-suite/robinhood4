/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef RBH_FIND_FILTERS_H
#define RBH_FIND_FILTERS_H

#include "parser.h"

#include <robinhood/filter.h>

/**
 * shell_regex2filter - build a filter from a shell regex
 *
 * @param predicate     a predicate
 * @param shell_regex   a shell regex
 * @param regex_options an ORred combination of enum filter_regex_option
 *
 * @return              a pointer to a newly allocated struct filter, or NULL on
 *                      error
 */
struct rbh_filter *
shell_regex2filter(enum predicate predicate, const char *shell_regex,
                   unsigned int regex_options);

/**
 * Build a filter for the -[acm]min predicate
 *
 * @param predicate one of PRED_[ACM]MIN
 * @param minutes   a string representing a number of minutes, optionnally
 *                  prefixed with either a '+' or '-' sign
 *
 * @return          a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
xmin2filter(enum predicate predicate, const char *minutes);

/**
 * Build a filter for the -[acm]time predicate
 *
 * @param predicate one of PRED_[ACM]TIME
 * @param days      a string representing a number of days, optionnally prefixed
 *                  with either a '+' or '-' sign
 *
 * @return          a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
xtime2filter(enum predicate predicate, const char *days);

#endif
