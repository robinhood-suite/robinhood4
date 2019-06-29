/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_FILTER_H
#define ROBINHOOD_FILTER_H

/** @file
 * Filters are meant to abstract predicates over the properties of an fsentry
 *
 * There are two type of filters: comparison filters, and logical filters.
 *
 * Comparison filters represent a single predicate:
 *     an fsentry's name matches '.*.c'
 *
 * They are made of three parts:
 *   - a field: "an fsentry's name";
 *   - an operator: "matches";
 *   - a value: "'.*.c'".
 *
 * Logical filters are combinations of other filters:
 *     (filter_A and filter_B) or not filter_C
 *
 * They are made of two parts:
 *    - an operator: and / or / not;
 *    - a list of filters.
 *
 * To distinguish one type from another, one needs only to look at the operator
 * in the struct filter (there are inline functions for this).
 */

#include <stdbool.h>
#include <time.h>

enum rbh_filter_operator {
    /* Comparison */
    RBH_FOP_EQUAL,
    RBH_FOP_LOWER_THAN,
    RBH_FOP_LOWER_OR_EQUAL,
    RBH_FOP_GREATER_THAN,
    RBH_FOP_GREATER_OR_EQUAL,
    RBH_FOP_IN,
    RBH_FOP_REGEX,
    RBH_FOP_BITS_ANY_SET,
    RBH_FOP_BITS_ALL_SET,
    RBH_FOP_BITS_ANY_CLEAR,
    RBH_FOP_BITS_ALL_CLEAR,

    /* Logical */
    RBH_FOP_AND,
    RBH_FOP_OR,
    RBH_FOP_NOT,

    /* Aliases (used to distinguish comparison filters from logical ones) */
    RBH_FOP_COMPARISON_MIN = RBH_FOP_EQUAL,
    RBH_FOP_COMPARISON_MAX = RBH_FOP_BITS_ALL_CLEAR,
    RBH_FOP_LOGICAL_MIN = RBH_FOP_AND,
    RBH_FOP_LOGICAL_MAX = RBH_FOP_NOT,
};

/**
 * Is \p op a comparison operator?
 *
 * @param op    the operator to test
 *
 * @return      true if \p op is a comparison operator, false otherwise
 */
static inline bool
rbh_is_comparison_operator(enum rbh_filter_operator op)
{
    return RBH_FOP_COMPARISON_MIN <= op && op <= RBH_FOP_COMPARISON_MAX;
}

/**
 * Is \p op a logical operator?
 *
 * @param op    the operator to test
 *
 * @return      true if \p op is a logical operator, false otherwise
 */
static inline bool
rbh_is_logical_operator(enum rbh_filter_operator op)
{
    return RBH_FOP_LOGICAL_MIN <= op && op <= RBH_FOP_LOGICAL_MAX;
}

/* TODO: (WIP) support every possible filter field */
enum rbh_filter_field {
    RBH_FF_ID = 0,
    RBH_FF_PARENT_ID,
    RBH_FF_ATIME,
    RBH_FF_MTIME,
    RBH_FF_CTIME,
    RBH_FF_NAME,
    RBH_FF_PATH,
    RBH_FF_TYPE,
};

enum rbh_filter_regex_option {
    RBH_FRO_CASE_INSENSITIVE = 0x0001,

    RBH_FRO_MAX = RBH_FRO_CASE_INSENSITIVE
};

enum rbh_filter_value_type {
    RBH_FVT_BINARY,
    RBH_FVT_INTEGER,
    RBH_FVT_STRING,
    RBH_FVT_REGEX,
    RBH_FVT_TIME,
    RBH_FVT_LIST,
};

struct rbh_filter_value {
    enum rbh_filter_value_type type;

    union {
        /* RBH_FVT_BINARY */
        struct {
            size_t size;
            const char *data;
        } binary;

        /* RBH_FVT_INTEGER */
        int integer;

        /* RBH_FVT_STRING */
        const char *string;

        /* RBH_FVT_REGEX */
        struct {
            unsigned int options;
            const char *string;
        } regex;

        /* RBH_FVT_TIME */
        time_t time;

        /* RBH_FVT_LIST */
        struct {
            size_t count;
            const struct rbh_filter_value *elements;
        } list;
    };
};

/**
 * NULL is a valid filter that matches everything.
 *
 * Conversely, the negation of a NULL filter:
 *
 * const struct rbh_filter *FILTER_NULL = NULL;
 * const struct rbh_filter FILTER_NOT_NULL = {
 *     .op = FOP_NOT,
 *     .logical = {
 *         .count = 1,
 *         .filters = &FILTER_NULL,
 *     },
 * };
 *
 * matches nothing.
 */
struct rbh_filter {
    enum rbh_filter_operator op;
    union {
        /* op is a comparison operator */
        struct {
            enum rbh_filter_field field;
            struct rbh_filter_value value;
        } compare;

        /* op is a logical operator */
        struct {
            size_t count;
            const struct rbh_filter * const *filters;
        } logical;
    };
};

/* The following helpers make it easier to manage memory and filters. */

/**
 * Any struct rbh_filter returned by the following functions can be freed with
 * a single call to free(). To recursively free a logical filter, one should use
 * rbh_filter_free(), but one must be sure that every sub-filter was allocated
 * dynamically or is at least safe to pass to free() (like NULL is).
 *
 * It is easy to mess things up when mixing statically and dynamically allocated
 * filters. If in doubt, either:
 *   - read the source code + write some tests;
 *   - only use statically allocated filters;
 *   - only use dynamically allocated filters and use rbh_filter_free() to free
 *     the top-most ones.
 */

/**
 * Create a filter that compares a field to a binary value
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare
 * @param size      the size of the binary value
 * @param data      the binary value to compare the field to
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 *
 * @error EINVAL    \p op is not a comparison operator, or is FOP_REGEX or
 *                  FOP_IN (which are invalid values)
 * @error ENOMEM    there was not enough memory available
 *
 * \p op may be any comparison operator except FOP_REGEX and FOP_IN
 */
struct rbh_filter *
rbh_filter_compare_binary_new(enum rbh_filter_operator op,
                              enum rbh_filter_field field, size_t size,
                              const char *data);

/**
 * Create a filter that compares a field to an integer
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare
 * @param integer   the integer to compare to
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 *
 * \p op may be any comparison operator except FOP_REGEX and FOP_IN
 */
struct rbh_filter *
rbh_filter_compare_integer_new(enum rbh_filter_operator op,
                               enum rbh_filter_field field, int integer);

/**
 * Create a filter that compares a field to a string
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare
 * @param string    the string to compare the field to
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p op was not a comparison operator, or was FOP_REGEX or
 *                  FOP_IN (which are invalid values)
 * @error ENOMEM    there was not enough memory available
 *
 * \p op may be any comparison operator except FOP_REGEX and FOP_IN
 */
struct rbh_filter *
rbh_filter_compare_string_new(enum rbh_filter_operator op,
                              enum rbh_filter_field field, const char *string);

/**
 * Create a filter that matches a field against a regex
 *
 * @param field         the field to compare
 * @param regex         the regex to compare the field to
 * @param regex_options a bitmask of enum rbh_filter_regex_option
 *
 * @return              a pointer to a newly allocated struct rbh_filter on
 *                      success, NULL on error and errno is set appropriately
 *
 * @error ENOMEM        there was not enough memory available
 */
struct rbh_filter *
rbh_filter_compare_regex_new(enum rbh_filter_field, const char *regex,
                             unsigned int regex_options);

/**
 * Create a filter that compares a field to a time_t
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare
 * @param time      the time_t to compare the field to
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p op was not a comparison operator, or was FOP_REGEX or
 *                  FOP_IN (which are invalid values)
 * @error ENOMEM    there was not enough memory available
 *
 * \p op may be any comparison operator except FOP_REGEX and FOP_IN
 */
struct rbh_filter *
rbh_filter_compare_time_new(enum rbh_filter_operator op,
                            enum rbh_filter_field field, time_t time);

/**
 * Create a filter that compares a field to a list of values
 *
 * @param field     the field to compare
 * @param count     the number of elements in values
 * @param values    an array of struct rbh_filter_value to compare field to
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 * @error EINVAL    one of the value of \p values is malformed (eg. its type is
 *                  not a valid enum rbh_filter_value_type).
 */
struct rbh_filter *
rbh_filter_compare_list_new(enum rbh_filter_field field, size_t count,
                            const struct rbh_filter_value values[]);

/**
 * Compose multiple filters with the AND operator
 *
 * @param filters   an array of pointers the filter filters to compose
 * @param count     the number of filters in \p filters
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 *
 * Note that calling rbh_filter_free() on the returned filter will call free()
 * on every element of \p filters and their sub-filters (if applicable).
 */
struct rbh_filter *
rbh_filter_and_new(const struct rbh_filter * const filters[], size_t count);

/**
 * Compose multiple filters with the OR operator
 *
 * @param filters   an array of pointers the filter filters to compose
 * @param count     the number of filters in \p filters
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 *
 * The returned filter will hold references on the elements of \p filters (but
 * not on \p filters itself). Among other tings, this means that calling
 * rbh_filter_free() on the returned filter will call free() on every element of
 * \p filters (and their sub-filters if applicable).
 */
struct rbh_filter *
rbh_filter_or_new(const struct rbh_filter * const filters[], size_t count);

/**
 * Negate a filter
 *
 * @param filter    the filter to negate
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 *
 *
 * The returned filter will hold references on \p filter. Among other tings,
 * this means that calling rbh_filter_free() on the returned filter will call
 * free() on \p filter (and their sub-filters if applicable).
 */
struct rbh_filter *
rbh_filter_not_new(const struct rbh_filter *filter);

/**
 * Validate a number of things about a filter
 *
 * @param filter    the filter to validate
 *
 * @return          0 if \p filter is valid, -1 if not and errno is set
 *                  appropriately
 *
 * @error EINVAL    \p filter is not a valid filter (it does not make sense)
 * @error ENOTSUP   \p filter uses a construct that makes sense but is not
 *                  supported (yet).
 */
int
rbh_filter_validate(const struct rbh_filter *filter);

/**
 * Pack and clone a filter
 *
 * @param filter    the filter to pack and clone
 *
 * @return          a newly allocated struct rbh_filter that is equivalent to
 *                  \p filter on success, NULL on error and errno is set
 *                  appropriately
 *
 * @error ENOMEM    there was not enough memory available
 *
 * Some parts of \p filter may be squashed, others ignored, so that you may not
 * assume anything of the content the returned filter. The only valid assumption
 * you can make is that the returned filter is semantically identical to
 * \p filter and can safely be passed to rbh_filter_free().
 *
 * As an example, in the following snippet:
 *
 * const struct rbh_filter not_null = {
 *     .op = FOP_NOT,
 *     .logical = {
 *         .count = 1,
 *         .filters = &NULL,
 *     },
 * };
 * const struct rbh_filter not_not_null = {
 *     .op = FOP_NOT,
 *     .logical = {
 *         .count = 1,
 *         .filters = &not_null,
 *     },
 * };
 * struct rbh_filter *clone = rbh_filter_clone(&not_not_null);
 *
 * It is quite possible that `clone' is NULL, but you should not rely on this.
 */
struct rbh_filter *
rbh_filter_clone(const struct rbh_filter *filter);

/**
 * Free a filter
 *
 * @param filter    the filter to free
 *
 * \p filter (and its subfilters if it is a logical filter) must have been
 * dynamically allocated (or be NULL).
 */
void
rbh_filter_free(struct rbh_filter *filter);

#endif
