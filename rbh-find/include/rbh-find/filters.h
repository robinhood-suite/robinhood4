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
 * shell_regex2filter - build a filter from a shell regex
 *
 * @param field         a field to filter
 * @param shell_regex   a shell regex
 * @param regex_options an ORred combination of enum filter_regex_option
 *
 * @return              a pointer to a newly allocated struct filter, or NULL on
 *                      error
 */
struct rbh_filter *
shell_regex2filter(const struct rbh_filter_field *field,
                   const char *shell_regex, unsigned int regex_options);

/**
 * regex2filter - build a filter from a regex, will call
 * shell_regex2filter for the actual filter creation
 *
 * @param predicate     a predicate
 * @param shell_regex   a shell regex
 * @param regex_options an ORred combination of enum filter_regex_option
 *
 * @return              a pointer to a newly allocated struct filter, or NULL on
 *                      error
 */
struct rbh_filter *
regex2filter(enum predicate predicate, const char *shell_regex,
             unsigned int regex_options);

/**
 * numeric2filter - build a filter from a string representing a uint64_t value.
 * If the given value is preceded with a '+' or '-', will filter entries
 * with \p field greater or lower than \p _numeric.
 *
 * @param field         a field to filter
 * @param _numeric      a string representing a uint64_t, optionnally prefixed
 *                      with either a '+' or '-' sign
 *
 * @return              a pointer to a newly allocated struct filter, or NULL on
 *                      error
 */
struct rbh_filter *
numeric2filter(const struct rbh_filter_field *field, const char *_numeric);

/**
 * number2filter - build a filter from a string representing a uint64_t value.
 *
 * @param predicate     a predicate
 * @param _numeric      a string representing a uint64_t
 *
 * @return              a pointer to a newly allocated struct filter, or NULL on
 *                      error
 */
struct rbh_filter *
number2filter(enum predicate predicate, const char *_numeric);

/**
 * epoch2filter - build a filter from a string representing a uint64_t value.
 * If the given value is preceded with a '+' or '-', will filter entries
 * with \p field greater or lower than \p _numeric. Else, it will filter entries
 * with \p field lower or equal to \p _epoch.
 *
 * @param field         a field to filter
 * @param _epoch        a string representing a uint64_t, optionnally prefixed
 *                      with either a '+' or '-' sign
 *
 * @return              a pointer to a newly allocated struct filter, or NULL on
 *                      error
 */
struct rbh_filter *
epoch2filter(const struct rbh_filter_field *field, const char *_epoch);

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
 * Extract the size parameters from a standard size filter string.
 * It must follow this pattern: `[+-]\d+[cwbkMGT]`
 *
 * @param _size      a string representing a size following the above pattern
 * @param operator   a character that will be either `+`, `-` or 0 if the other
 *                   two have not been found
 * @param unit_size  the unit size specified in the \p _size parameter, 512 by
 *                   default if none is specified
 * @param size       the size specified in the string
 *
 * Exit on error
 */
void
get_size_parameters(const char *_size, char *operator, uint64_t *unit_size,
                    uint64_t *size);

/**
 * Build a filter for predicates corresponding to a size by following
 * GNU-find's logic.
 *
 * For instance, the following will match:
 *  - `+3k` -> all entries of size larger than 3k
 *  - `-1M` -> all entries of size lower or equal to 0M
 *  - `2T` -> all entries of size in interval ]1T; 2T + 1[
 *
 * @param field     a field to filter
 * @param size      a string representing a size
 *
 * @return          a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
size2filter(const struct rbh_filter_field *field, const char *size);

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
 * Build a filter for the -empty predicate
 *
 * For now, works only on files. The behavior for the directories will be added
 * in the future.
 *
 * @return          a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
empty2filter();

/**
 * Build a filter for the -user predicate
 *
 * @param username  a string representing a username
 *
 * @return          a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
username2filter(const char *username);

/**
 * Build a filter for the -nouser predicate
 *
 * @return          a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
nouser2filter();

/**
 * Build a filter for the -group predicate
 *
 * @param groupname a string representing a username
 *
 * @return          a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
groupname2filter(const char *groupname);

/**
 * Build a filter for the -nogroup predicate
 *
 * @return          a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
nogroup2filter();

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
 * Build a filter for the -xattr predicate
 *
 * @param xattr_field  field name of the xattr to use
 *
 * @return             a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
xattr2filter(const char *xattr_field);

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
 * Create an array filter for the ELEMMATCH operator, combining 2 comparison
 * filters for the checks.
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
filter_array_compose(struct rbh_filter *left, struct rbh_filter *right);

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
