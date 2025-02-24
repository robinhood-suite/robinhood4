/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_FILTER_H
#define ROBINHOOD_FILTER_H

/** @file
 * Filters are meant to abstract predicates over the properties of an fsentry
 *
 * \section types Types of filters
 *
 * There are three type of filters: comparison filters, logical filters, and
 * the NULL filter.
 *
 * To distinguish comparison filters from logical filters, one needs only to
 * look at its operator (cf. rbh_is_comparison_operator() and
 * rbh_is_logical_operator()).
 *
 * \subsection comparison Comparison filters
 *
 * Comparison filters represent a single predicate:
 *
 *     an fsentry's name matches '.*.c'
 *
 * They are made of three parts:
 *   - a field: \c "an fsentry's name";
 *   - an operator: \c "matches";
 *   - a value: \c ".*.c".
 *
 * Here is a table that presents valid combinations of
 * comparison operator / value type:
 *
 * |          | EQUAL | LOWER/GREATER | REGEX | IN | BITS |
 * |----------|:-----:|:-------------:|:-----:|:--:|:----:|
 * | BINARY   |   X   |       X       |       |    |      |
 * | INTEGERS |   X   |       X       |       |    |   X  |
 * | STRING   |   X   |       X       |       |    |      |
 * | REGEX    |   X   |       X       |   X   |    |      |
 * | SEQUENCE |   X   |       X       |       |  X |      |
 * | MAP      |   X   |       X       |       |    |      |
 *
 * Using LOWER/GREATER operators with any value type other than INTEGERS, while
 * considered to be a valid filter may yield different result depending on the
 * backend that will interpret them. Refrain from using them unless you know
 * what you are doing.
 *
 * \subsection logical Logical filters
 *
 * Logical filters are combinations of other filters:
 *
 *     (filter_A and filter_B) or not filter_C
 *
 * They are made of two parts:
 *    - an operator: \c "and", \c "or", \c "not";
 *    - a list of filters: \c "filter_A", \c "filter_B", and "filter_C".
 *
 * \subsection null NULL filter
 *
 * Technically, there is a third type of filters: the NULL filter.
 *
 * NULL is a valid filter that matches everything.
 *
 * Conversely, the negation of a NULL filter:
 *
 * ```
 * const struct rbh_filter *FILTER_NULL = NULL;
 * const struct rbh_filter FILTER_NOT_NULL = {
 *     .op = FOP_NOT,
 *     .logical = {
 *         .filters = &FILTER_NULL,
 *         .count = 1,
 *     },
 * };
 * ```
 *
 * matches nothing.
 */

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "robinhood/fsentry.h"
#include "robinhood/value.h"

/**
 * Filter operator
 *
 * There are two types of operators: comparison operators and logical operators.
 */
enum rbh_filter_operator {
    /* Comparison */
    RBH_FOP_EQUAL,
    RBH_FOP_STRICTLY_LOWER,
    RBH_FOP_LOWER_OR_EQUAL,
    RBH_FOP_STRICTLY_GREATER,
    RBH_FOP_GREATER_OR_EQUAL,
    RBH_FOP_REGEX,
    RBH_FOP_IN,
    RBH_FOP_EXISTS,
    RBH_FOP_BITS_ANY_SET,
    RBH_FOP_BITS_ALL_SET,
    RBH_FOP_BITS_ANY_CLEAR,
    RBH_FOP_BITS_ALL_CLEAR,

    /* Logical */
    RBH_FOP_AND,
    RBH_FOP_OR,
    RBH_FOP_NOT,

    /* Array */
    RBH_FOP_ELEMMATCH,

    /* Get */
    RBH_FOP_GET,

    /* Aliases (used to distinguish comparison filters from logical ones) */
    RBH_FOP_COMPARISON_MIN = RBH_FOP_EQUAL,
    RBH_FOP_COMPARISON_MAX = RBH_FOP_BITS_ALL_CLEAR,
    RBH_FOP_LOGICAL_MIN = RBH_FOP_AND,
    RBH_FOP_LOGICAL_MAX = RBH_FOP_NOT,
    RBH_FOP_ARRAY_MIN = RBH_FOP_ELEMMATCH,
    RBH_FOP_ARRAY_MAX = RBH_FOP_ELEMMATCH,
    RBH_FOP_GET_MIN = RBH_FOP_GET,
    RBH_FOP_GET_MAX = RBH_FOP_GET,
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

/**
 * Is \p op an array operator?
 *
 * @param op    the operator to test
 *
 * @return      true if \p op is an array operator, false otherwise
 */
static inline bool
rbh_is_array_operator(enum rbh_filter_operator op)
{
    return RBH_FOP_ARRAY_MIN <= op && op <= RBH_FOP_ARRAY_MAX;
}

/**
 * Is \p op a get operator?
 *
 * @param op    the operator to test
 *
 * @return      true if \p op is an array operator, false otherwise
 */
static inline bool
rbh_is_get_operator(enum rbh_filter_operator op)
{
    return RBH_FOP_GET_MIN <= op && op <= RBH_FOP_GET_MAX;
}

/**
 * A field on which to apply a filter
 */
struct rbh_filter_field {
    /** An fsentry property */
    enum rbh_fsentry_property fsentry;
    union {
        /** If \c fsentry is RBH_FP_STATX: a STATX_* or RBH_STATX_* macro */
        uint32_t statx;
        /** If \c fsentry is RBH_FP_*_XATTRS: NULL, or a string
         *
         * If NULL, the filter will be applied on the \c fsentry field itself.
         */
        const char *xattr;
    };
};

const struct rbh_filter_field *str2filter_field(const char *string);

/**
 * A filter, to be used with rbh_backend_filter()
 *
 */
struct rbh_filter {
    enum rbh_filter_operator op;
    union {
        struct {
            struct rbh_filter_field field;
            struct rbh_value value;
        } compare;

        struct {
            const struct rbh_filter * const *filters;
            size_t count;
        } logical;

        struct {
            struct rbh_filter_field field;
            /** Should only be composed of comparison filters. The filter field
             * specified in them is irrelevant.
             */
            const struct rbh_filter * const *filters;
            size_t count;
        } array;

        struct {
            struct rbh_filter *filter;
            /**
             * Comparison filter of the fsentry to retrieve to finalize the
             * filter
             */
            const struct rbh_filter *fsentry_to_get;
            struct rbh_filter_field field;
        } get;
    };
};

/* The following helpers make it easier to manage memory and filters.
 *
 * Any struct rbh_filter returned by the following functions can be freed with
 * a single call to free().
 */

/**
 * Create a filter that compares a field to a binary value
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare
 * @param data      the binary data to compare the field to
 * @param size      the length in bytes of \p data
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p op is not valid for a binary comparison
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_compare_binary_new(enum rbh_filter_operator op,
                              const struct rbh_filter_field *field,
                              const char *data, size_t size);

/**
 * Create a filter that compares a field to an uint32_t
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare
 * @param uint32    the integer to compare \p field to
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p op is not valid for an integer comparison
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_compare_uint32_new(enum rbh_filter_operator op,
                              const struct rbh_filter_field *field,
                              uint32_t uint32);

/**
 * Create a filter that compares a field to an uint64_t
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare
 * @param uint64    the integer to compare \p field to
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p op is not valid for an integer comparison
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_compare_uint64_new(enum rbh_filter_operator op,
                              const struct rbh_filter_field *field,
                              uint64_t uint64);

/**
 * Create a filter that compares a field to an int32_t
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare
 * @param int32     the integer to compare \p field to
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p op is not valid for an integer comparison
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_compare_int32_new(enum rbh_filter_operator op,
                             const struct rbh_filter_field *field,
                             int32_t int32);

/**
 * Create a filter that compares a field to an int64_t
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare
 * @param int64     the integer to compare \p field to
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p op is not valid for an integer comparison
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_compare_int64_new(enum rbh_filter_operator op,
                             const struct rbh_filter_field *field,
                             int64_t int64);

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
 * @error EINVAL    \p op is not valid for a string comparison
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_compare_string_new(enum rbh_filter_operator op,
                              const struct rbh_filter_field *field,
                              const char *string);

/**
 * Create a filter that matches a field against a regex
 *
 * @param op            the type of comparison to use
 * @param field         the field to compare
 * @param regex         the regex to compare the field to
 * @param regex_options a bitmask of enum rbh_filter_regex_option
 *
 * @return              a pointer to a newly allocated struct rbh_filter on
 *                      success, NULL on error and errno is set appropriately
 *
 * @error EINVAL        \p op is not valid for a regex comparison
 * @error ENOMEM        there was not enough memory available
 */
struct rbh_filter *
rbh_filter_compare_regex_new(enum rbh_filter_operator op,
                             const struct rbh_filter_field *field,
                             const char *regex, unsigned int regex_options);

/**
 * Create a filter that compares a field to a sequence of values
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare
 * @param values    an array of struct rbh_value to compare \p field to
 * @param count     the number of elements in \p values
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p op is not valid for a sequence comparison or one of the
 *                  elements of \p values is not a valid struct rbh_value
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_compare_sequence_new(enum rbh_filter_operator op,
                                const struct rbh_filter_field *field,
                                const struct rbh_value values[], size_t count);

/**
 * Create a filter that compares a field to a map
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare
 * @param pairs     an array of struct rbh_value_pair to compare \p field to
 * @param count     the number of elements in \p pairs
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p op is not valid for a map comparison or one of the
 *                  elements of \p pairs points at an invalid struct rbh_value
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_compare_map_new(enum rbh_filter_operator op,
                           const struct rbh_filter_field *field,
                           const struct rbh_value_pair pairs[], size_t count);

/**
 * Create a comparison filter
 *
 * @param op        the type of comparison to use
 * @param field     the field to compare to \p value
 * @param value     the value to compare to \p field
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p op and \p value are not compatible
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_compare_new(enum rbh_filter_operator op,
                       const struct rbh_filter_field *field,
                       const struct rbh_value *value);

/**
 * Create a filter that ANDs multiple filters
 *
 * @param filters   an array of filters to AND
 * @param count     the number of filters in \p filters
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_and_new(const struct rbh_filter * const filters[], size_t count);

/**
 * Create a filter that ORs multiple filters
 *
 * @param filters   an array of filters to OR
 * @param count     the number of filters in \p filters
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_or_new(const struct rbh_filter * const filters[], size_t count);

/**
 * Create a filter that negates another filter
 *
 * @param filter    the filter to negate
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_not_new(const struct rbh_filter *filter);

/**
 * Create a filter that checks a field exists
 *
 * @param field     the field whose existence to check
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_exists_new(const struct rbh_filter_field *field);

/**
 * Create a filter that checks a cell of an array matches multiple conditions
 *
 * @param field     the field corresponding to the array to check
 * @param filters   the filters to apply to the array
 * @param count     the number of filters to apply
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_array_elemmatch_new(const struct rbh_filter_field *field,
                               const struct rbh_filter * const *filters,
                               size_t count);

/**
 * Create a filter that need to be completed before using it.
 *
 * @param filter         the filter to complete
 * @param fsentry_to_get the filter of the fsentry to retrieve to complete the
 *                       the first filter
 * @param field          the field corresponding of the value to retrieve
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_get_new(struct rbh_filter *filter,
                   const struct rbh_filter *fsentry_to_get,
                   const struct rbh_filter_field *field);

/**
 * Validate a filter
 *
 * @param filter    the filter to validate
 *
 * @return          0 if \p filter is valid, -1 otherwise and errno is set
 *                  appropriately
 *
 * @error EINVAL    \p filter is invalid
 */
int
rbh_filter_validate(const struct rbh_filter *filter);

/**
 * Clone a filter
 *
 * @param filter    the filter to clone
 *
 * @return          a pointer to a newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_filter *
rbh_filter_clone(const struct rbh_filter *filter);

#endif
