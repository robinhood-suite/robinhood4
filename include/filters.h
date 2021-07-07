/* This file is part of rbh-find
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
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

/**
 * Build a filter for the -type predicate
 *
 * @param filetype  a string representing a filetype
 *
 * @return          a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
filetype2filter(const char *filetype);

/**
 * Build a filter for the -size predicate
 *
 * @param filesize  a string representing a size
 *
 * @return          a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
filesize2filter(const char *filesize);

/**
 * Build a filter for the -perm predicate
 *
 * @param mode_arg a string representing an octal or symbol mode
 *
 * @return         a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
mode2filter(const char *mode_arg);

/**
 * AND two dynamically allocated filters
 *
 * @param left  a pointer to a dynamically allocated struct rbh_filter
 * @param right a pointer to a dynamically allocated struct rbh_filter
 *
 * @return      a pointer to a newly allocated struct rbh_filter
 *
 * \p left and \p right should not be accessed anymore after this function is
 * called.
 *
 * Exit on error
 */
struct rbh_filter *
filter_and(struct rbh_filter *left, struct rbh_filter *right);

/**
 * OR two dynamically allocated filters
 *
 * @param left  a pointer to a dynamically allocated struct rbh_filter
 * @param right a pointer to a dynamically allocated struct rbh_filter
 *
 * @return      a pointer to a newly allocated struct rbh_filter
 *
 * \p left and \p right should not be accessed anymore after this function is
 * called.
 *
 * Exit on error
 */
struct rbh_filter *
filter_or(struct rbh_filter *left, struct rbh_filter *right);

/**
 * Negate a dynamically allocated filter
 *
 * @param filter    a pointer to a dynamically allocated struct rbh_filter
 *
 * @return          a pointer to a newly allocated struct rbh_filter
 *
 * \p filter should not be accessed anymore after this function is called.
 *
 * Exit on error
 */
struct rbh_filter *
filter_not(struct rbh_filter *filter);

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
