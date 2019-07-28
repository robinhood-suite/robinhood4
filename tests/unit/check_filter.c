/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>
#include <stdlib.h>

#include "check-compat.h"
#include "robinhood/filter.h"

/*----------------------------------------------------------------------------*
 |                               tests helpers                                |
 *----------------------------------------------------------------------------*/

static const char *
filter_value_type2str(enum rbh_filter_value_type type)
{
    switch (type) {
    case RBH_FVT_BINARY:
        return "RBH_FVT_BINARY";
    case RBH_FVT_INTEGER:
        return "RBH_FVT_INTEGER";
    case RBH_FVT_STRING:
        return "RBH_FVT_STRING";
    case RBH_FVT_REGEX:
        return "RBH_FVT_REGEX";
    case RBH_FVT_TIME:
        return "RBH_FVT_TIME";
    case RBH_FVT_LIST:
        return "RBH_FVT_LIST";
    default:
        return "unknown";
    }
}

#define _ck_assert_filter_value_type(X, OP, Y) do { \
    ck_assert_msg((X) OP (Y), ": %s is %s, %s is %s", \
                  #X, filter_value_type2str(X), \
                  #Y, filter_value_type2str(Y)); \
} while (0)

#define _ck_assert_filter_value(X, OP, Y) do { \
    _ck_assert_filter_value_type((X)->type, OP, (Y)->type); \
    switch ((X)->type) { \
    case RBH_FVT_BINARY: \
        _ck_assert_uint((X)->binary.size, OP, (Y)->binary.size); \
        _ck_assert_mem((X)->binary.data, OP, (Y)->binary.data, \
                       (X)->binary.size); \
        break; \
    case RBH_FVT_INTEGER: \
        _ck_assert_int((X)->integer, OP, (Y)->integer); \
        break; \
    case RBH_FVT_STRING: \
        _ck_assert_str((X)->string, OP, (Y)->string, 0, 0); \
        break; \
    case RBH_FVT_REGEX: \
        _ck_assert_uint((X)->regex.options, OP, (Y)->regex.options); \
        _ck_assert_str((X)->regex.string, OP, (Y)->regex.string, 0, 0); \
        break; \
    case RBH_FVT_TIME: \
        _ck_assert_mem(&(X)->time, OP, &(Y)->time, sizeof((X)->time)); \
        break; \
    case RBH_FVT_LIST: \
        _ck_assert_uint((X)->list.count, OP, (Y)->list.count); \
        /* Recursion has to be done manually */ \
    } \
} while (0)

#define ck_assert_filter_value_eq(X, Y) _ck_assert_filter_value(X, ==, Y)

static const char *
filter_operator2str(enum rbh_filter_operator op)
{
    switch (op) {
    case RBH_FOP_EQUAL:
        return "RBH_FOP_EQUAL";
    case RBH_FOP_LOWER_THAN:
        return "RBH_FOP_LOWER_THAN";
    case RBH_FOP_LOWER_OR_EQUAL:
        return "RBH_FOP_LOWER_OR_EQUAL";
    case RBH_FOP_GREATER_THAN:
        return "RBH_FOP_GREATER_THAN";
    case RBH_FOP_GREATER_OR_EQUAL:
        return "RBH_FOP_GREATER_OR_EQUAL";
    case RBH_FOP_IN:
        return "RBH_FOP_IN";
    case RBH_FOP_REGEX:
        return "RBH_FOP_REGEX";
    case RBH_FOP_BITS_ANY_SET:
        return "RBH_FOP_BITS_ANY_SET";
    case RBH_FOP_BITS_ALL_SET:
        return "RBH_FOP_BITS_ALL_SET";
    case RBH_FOP_BITS_ANY_CLEAR:
        return "RBH_FOP_BITS_ANY_CLEAR";
    case RBH_FOP_BITS_ALL_CLEAR:
        return "RBH_FOP_BITS_ALL_CLEAR";
    case RBH_FOP_AND:
        return "RBH_FOP_AND";
    case RBH_FOP_OR:
        return "RBH_FOP_OR";
    case RBH_FOP_NOT:
        return "RBH_FOP_NOT";
    default:
        return "unknown";
    }
}

#define _ck_assert_filter_operator(X, OP, Y) do { \
    ck_assert_msg((X) OP (Y), "%s is %s, %s is %s", \
                  #X, filter_operator2str(X), #Y, filter_operator2str(Y)); \
} while (0);

#define ck_assert_filter_operator_eq(X, Y) _ck_assert_filter_operator(X, ==, Y)

static const char *
filter_field2str(enum rbh_filter_field field)
{
    switch (field) {
    case RBH_FF_ID:
        return "RBH_FF_ID";
    case RBH_FF_PARENT_ID:
        return "RBH_FF_PARENT_ID";
    case RBH_FF_ATIME:
        return "RBH_FF_ATIME";
    case RBH_FF_MTIME:
        return "RBH_FF_MTIME";
    case RBH_FF_CTIME:
        return "RBH_FF_CTIME";
    case RBH_FF_NAME:
        return "RBH_FF_NAME";
    case RBH_FF_TYPE:
        return "RBH_FF_TYPE";
    default:
        return "unknown";
    }
}

#define _ck_assert_filter_field(X, OP, Y) do { \
    ck_assert_msg((X) OP (Y), "%s is %s, %s is %s", \
                  #X, filter_field2str(X), #Y, filter_field2str(Y)); \
} while (0);

#define _ck_assert_comparison_filter(X, OP, Y) do { \
    _ck_assert_filter_field((X)->compare.field, OP, (X)->compare.field); \
    _ck_assert_filter_value(&(X)->compare.value, OP, &(Y)->compare.value); \
} while (0);

#define _ck_assert_filter(X, OP, Y) do { \
    _ck_assert_filter_operator((X)->op, OP, (Y)->op); \
    if (rbh_is_comparison_operator((X)->op)) { \
        _ck_assert_comparison_filter(X, OP, Y); \
    } else { \
        _ck_assert_uint((X)->logical.count, OP, (Y)->logical.count); \
        /* Recursion has to bedone manually */ \
    } \
} while (0)

#define ck_assert_filter_eq(X, Y) _ck_assert_filter(X, ==, Y)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

static const struct rbh_filter_value filter_values[] = {
    {
        .type = RBH_FVT_BINARY,
        .binary = {
            .size = 16,
            .data = "abcdefghijklmnop",
        },
    },
    {
        .type = RBH_FVT_INTEGER,
        .integer = 0,
    },
    {
        .type = RBH_FVT_INTEGER,
        .integer = INT_MAX,
    },
    {
        .type = RBH_FVT_STRING,
        .string = "",
    },
    {
        .type = RBH_FVT_STRING,
        .string = "abcdefghijklmno",
    },
    {
        .type = RBH_FVT_REGEX,
        .regex = {
            .options = 0,
            .string = "abcdefghijklmno",
        },
    },
    {
        .type = RBH_FVT_REGEX,
        .regex = {
            .options = RBH_FRO_CASE_INSENSITIVE,
            .string = "AbCdEfGhIjKlMnO",
        },
    },
    {
        .type = RBH_FVT_TIME,
        .time = 0,
    },
};

static const struct rbh_filter comparison_filters[] = {
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = RBH_FF_ID,
            .value = {
                .type = RBH_FVT_BINARY,
                .binary = {
                    .size = 16,
                    .data = "abcdefghijklmnop",
                },
            },
        },
    },
    {
        .op = RBH_FOP_LOWER_THAN,
        .compare = {
            .field = RBH_FF_PARENT_ID,
            .value = {
                .type = RBH_FVT_INTEGER,
                .integer = INT_MAX,
            },
        },
    },
    {
        .op = RBH_FOP_LOWER_OR_EQUAL,
        .compare = {
            .field = RBH_FF_ATIME,
            .value = {
                .type = RBH_FVT_STRING,
                .string = "",
            },
        },
    },
    {
        .op = RBH_FOP_GREATER_THAN,
        .compare = {
            .field = RBH_FF_MTIME,
            .value = {
                .type = RBH_FVT_STRING,
                .string = "abcdefghijklmno",
            }
        },
    },
    {
        .op = RBH_FOP_REGEX,
        .compare = {
            .field = RBH_FF_CTIME,
            .value = {
                .type = RBH_FVT_REGEX,
                .regex = {
                    .options = 0,
                    .string = "abcdefghijklmno",
                },
            },
        },
    },
    {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = RBH_FF_NAME,
            .value = {
                .type = RBH_FVT_TIME,
                .time = 0,
            },
        },
    },
    {
        .op = RBH_FOP_IN,
        .compare = {
            .field = RBH_FF_TYPE,
            .value = {
                .type = RBH_FVT_LIST,
                .list = {
                    .count = 0,
                    .elements = NULL,
                },
            },
        },
    },
    {
        .op = RBH_FOP_IN,
        .compare = {
            .field = RBH_FF_ID,
            .value = {
                .type = RBH_FVT_LIST,
                .list = {
                    .count = ARRAY_SIZE(filter_values),
                    .elements = filter_values,
                },
            },
        },
    }
};

/*----------------------------------------------------------------------------*
 |                      rbh_filter_compare_binary_new()                       |
 *----------------------------------------------------------------------------*/

START_TEST(rfcbn_basic)
{
    struct rbh_filter *filter;

    filter = rbh_filter_compare_binary_new(RBH_FOP_EQUAL, RBH_FF_ID, 16,
                                       "abcdefghijklmnop");
    ck_assert_ptr_nonnull(filter);

    ck_assert_int_eq(filter->op, RBH_FOP_EQUAL);
    ck_assert_int_eq(filter->compare.field, RBH_FF_ID);
    ck_assert_int_eq(filter->compare.value.type, RBH_FVT_BINARY);
    ck_assert_int_eq(filter->compare.value.binary.size, 16);
    ck_assert_mem_eq(filter->compare.value.binary.data, "abcdefghijklmnop", 16);

    rbh_filter_free(filter);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                      rbh_filter_compare_integer_new()                      |
 *----------------------------------------------------------------------------*/

START_TEST(rfcin_basic)
{
    struct rbh_filter *filter;

    filter = rbh_filter_compare_integer_new(RBH_FOP_EQUAL, RBH_FF_ID, 1234);
    ck_assert_ptr_nonnull(filter);

    ck_assert_int_eq(filter->op, RBH_FOP_EQUAL);
    ck_assert_int_eq(filter->compare.field, RBH_FF_ID);
    ck_assert_int_eq(filter->compare.value.type, RBH_FVT_INTEGER);
    ck_assert_int_eq(filter->compare.value.integer, 1234);

    rbh_filter_free(filter);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                      rbh_filter_compare_string_new()                       |
 *----------------------------------------------------------------------------*/

START_TEST(rfcsn_basic)
{
    struct rbh_filter *filter;

    filter = rbh_filter_compare_string_new(RBH_FOP_EQUAL, RBH_FF_NAME,
                                           "abcdefg");
    ck_assert_ptr_nonnull(filter);

    ck_assert_int_eq(filter->op, RBH_FOP_EQUAL);
    ck_assert_int_eq(filter->compare.field, RBH_FF_NAME);
    ck_assert_int_eq(filter->compare.value.type, RBH_FVT_STRING);
    ck_assert_str_eq(filter->compare.value.string, "abcdefg");

    rbh_filter_free(filter);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                       rbh_filter_compare_regex_new()                       |
 *----------------------------------------------------------------------------*/

START_TEST(rfcrn_basic)
{
    struct rbh_filter *filter;

    filter = rbh_filter_compare_regex_new(RBH_FF_NAME, "abcdefg", ~0U);
    ck_assert_ptr_nonnull(filter);

    ck_assert_int_eq(filter->op, RBH_FOP_REGEX);
    ck_assert_int_eq(filter->compare.field, RBH_FF_NAME);
    ck_assert_int_eq(filter->compare.value.type, RBH_FVT_REGEX);
    ck_assert_int_eq(filter->compare.value.regex.options, ~0U);
    ck_assert_str_eq(filter->compare.value.regex.string, "abcdefg");

    rbh_filter_free(filter);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                       rbh_filter_compare_time_new()                        |
 *----------------------------------------------------------------------------*/

START_TEST(rfctn_basic)
{
    struct rbh_filter *filter;
    time_t time = 0;

    filter = rbh_filter_compare_time_new(RBH_FOP_EQUAL, RBH_FF_ATIME, time);
    ck_assert_ptr_nonnull(filter);

    ck_assert_int_eq(filter->op, RBH_FOP_EQUAL);
    ck_assert_int_eq(filter->compare.field, RBH_FF_ATIME);
    ck_assert_int_eq(filter->compare.value.type, RBH_FVT_TIME);
    ck_assert_int_eq(filter->compare.value.time, time);

    rbh_filter_free(filter);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                       rbh_filter_compare_list_new()                        |
 *----------------------------------------------------------------------------*/

START_TEST(rfcln_basic)
{
    struct rbh_filter *filter;

    filter = rbh_filter_compare_list_new(RBH_FF_ID, ARRAY_SIZE(filter_values),
                                         filter_values);
    ck_assert_ptr_nonnull(filter);

    ck_assert_int_eq(filter->op, RBH_FOP_IN);
    ck_assert_int_eq(filter->compare.field, RBH_FF_ID);
    ck_assert_int_eq(filter->compare.value.type, RBH_FVT_LIST);
    ck_assert_int_eq(filter->compare.value.list.count,
                     ARRAY_SIZE(filter_values));
    for (size_t i = 0; i < filter->compare.value.list.count; i++)
        ck_assert_filter_value_eq(&filter->compare.value.list.elements[i],
                                  &filter_values[i]);

    rbh_filter_free(filter);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_filter_and_new()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rfan_null)
{
    const struct rbh_filter *filters[1] = { NULL };
    struct rbh_filter *filter;

    filter = rbh_filter_and_new(filters, 1);
    ck_assert_ptr_nonnull(filter);

    ck_assert_int_eq(filter->logical.count, 1);
    ck_assert_ptr_ne(filter->logical.filters, filters);
    ck_assert_ptr_eq(filter->logical.filters[0], NULL);

    rbh_filter_free(filter);
}
END_TEST

START_TEST(rfan_many)
{
    const struct rbh_filter *filters[ARRAY_SIZE(comparison_filters)];
    struct rbh_filter *filter;

    for (size_t i = 0; i < ARRAY_SIZE(comparison_filters); i++)
        filters[i] = &comparison_filters[i];

    filter = rbh_filter_and_new(filters, ARRAY_SIZE(comparison_filters));
    ck_assert_ptr_nonnull(filter);

    ck_assert_filter_operator_eq(filter->op, RBH_FOP_AND);
    ck_assert_uint_eq(filter->logical.count, ARRAY_SIZE(filters));
    ck_assert_ptr_ne(filter->logical.filters, filters);
    for (size_t i = 0; i < ARRAY_SIZE(filters); i++) {
        ck_assert_ptr_eq(filter->logical.filters[i], filters[i]);
        ck_assert_filter_eq(filter->logical.filters[i], &comparison_filters[i]);
    }

    free(filter);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_filter_or_new()                             |
 *----------------------------------------------------------------------------*/

/* The underlying implementation of filter_or() is the same as filter_and()'s.
 * There is no need to test is extensively.
 */
START_TEST(rfon_many)
{
    const struct rbh_filter *filters[3];
    struct rbh_filter *filter;

    for (size_t i = 0; i < ARRAY_SIZE(filters); i++)
        filters[i] = rbh_filter_clone(&comparison_filters[i]);

    filter = rbh_filter_or_new(filters, ARRAY_SIZE(filters));
    ck_assert_int_eq(filter->op, RBH_FOP_OR);

    rbh_filter_free(filter);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_filter_not_new()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rfnn_basic)
{
    struct rbh_filter *negated;
    struct rbh_filter *filter;

    filter = rbh_filter_clone(&comparison_filters[0]);
    ck_assert_ptr_nonnull(filter);

    negated = rbh_filter_not_new(filter);
    ck_assert_ptr_nonnull(negated);
    ck_assert_int_eq(negated->op, RBH_FOP_NOT);
    ck_assert_int_eq(negated->logical.count, 1);
    ck_assert_ptr_eq(negated->logical.filters[0], filter);

    rbh_filter_free(negated);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                             rbh_filter_clone()                             |
 *----------------------------------------------------------------------------*/

START_TEST(rfc_null)
{
    ck_assert_ptr_null(rbh_filter_clone(NULL));
}
END_TEST

START_TEST(rfc_comparison)
{
    struct rbh_filter *clone;

    clone = rbh_filter_clone(&comparison_filters[_i]);
    ck_assert_filter_eq(&comparison_filters[_i], clone);

    rbh_filter_free(clone);
}
END_TEST

START_TEST(rfc_logical)
{
    const struct rbh_filter *filters[ARRAY_SIZE(comparison_filters)];
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_AND,
        .logical = {
            .count = ARRAY_SIZE(comparison_filters),
            .filters = filters,
        },
    };
    struct rbh_filter *clone;

    for (size_t i = 0; i < ARRAY_SIZE(comparison_filters); i++)
        filters[i] = &comparison_filters[i];

    clone = rbh_filter_clone(&FILTER);
    ck_assert_ptr_nonnull(clone);
    ck_assert_ptr_ne(clone, &FILTER);
    ck_assert_filter_eq(clone, &FILTER);

    for (size_t i = 0; i < ARRAY_SIZE(comparison_filters); i++)
        ck_assert_filter_eq(clone->logical.filters[i], &comparison_filters[i]);

    rbh_filter_free(clone);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("filter interface");

    tests = tcase_create("rbh_filter_compare_binary_new");
    tcase_add_test(tests, rfcbn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_compare_integer_new");
    tcase_add_test(tests, rfcin_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_compare_string_new");
    tcase_add_test(tests, rfcsn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_compare_regex_new");
    tcase_add_test(tests, rfcrn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_compare_time_new");
    tcase_add_test(tests, rfctn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_compare_list_new");
    tcase_add_test(tests, rfcln_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_and");
    tcase_add_test(tests, rfan_null);
    tcase_add_test(tests, rfan_many);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_or");
    tcase_add_test(tests, rfon_many);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_not");
    tcase_add_test(tests, rfnn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_clone");
    tcase_add_test(tests, rfc_null);
    tcase_add_loop_test(tests, rfc_comparison, 0,
                        ARRAY_SIZE(comparison_filters));
    tcase_add_test(tests, rfc_logical);

    suite_add_tcase(suite, tests);

    return suite;
}

int
main(void)
{
    int number_failed;
    Suite *suite;
    SRunner *runner;

    suite = unit_suite();
    runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
