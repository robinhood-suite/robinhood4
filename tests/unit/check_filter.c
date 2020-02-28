/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "robinhood/filter.h"

#include "check-compat.h"
#include "check_macros.h"
#include "utils.h"

/*----------------------------------------------------------------------------*
 |                               tests helpers                                |
 *----------------------------------------------------------------------------*/

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

#define _ck_assert_comparison_filter(X, OP, Y, NULLEQ, NULLNE) do { \
    _ck_assert_filter_field((X)->compare.field, OP, (X)->compare.field); \
    _ck_assert_value(&(X)->compare.value, OP, &(Y)->compare.value); \
} while (0);

#define _ck_assert_filter(X, OP, Y, NULLEQ, NULLNE) do { \
    if ((X) == NULL || (Y) == NULL) { \
        _ck_assert_ptr(X, OP, Y); \
        break; \
    } \
    _ck_assert_filter_operator((X)->op, OP, (Y)->op); \
    if (rbh_is_comparison_operator((X)->op)) { \
        _ck_assert_comparison_filter(X, OP, Y, NULLEQ, NULLNE); \
    } else { \
        _ck_assert_uint((X)->logical.count, OP, (Y)->logical.count); \
        /* Recursion has to bedone manually */ \
    } \
} while (0)

#define ck_assert_filter_eq(X, Y) _ck_assert_filter(X, ==, Y, 1, 0)

/*----------------------------------------------------------------------------*
 |                          rbh_filter_compare_new()                          |
 *----------------------------------------------------------------------------*/

START_TEST(rfcn_basic)
{
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = RBH_FF_ID,
            .value = {
                .type = RBH_VT_BINARY,
                .binary = {
                    .data = "abcdefghijklmnop",
                    .size = 16,
                },
            },
        },
    };
    struct rbh_filter *filter;

    filter = rbh_filter_compare_new(FILTER.op, FILTER.compare.field,
                                    &FILTER.compare.value);
    ck_assert_ptr_nonnull(filter);

    ck_assert_filter_eq(filter, &FILTER);
    free(filter);
}
END_TEST

START_TEST(rfcn_bad_operator)
{
    const struct rbh_value VALUE = {};

    errno = 0;
    ck_assert_ptr_null(rbh_filter_compare_new(-1, RBH_FF_ID, &VALUE));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfcn_in_without_sequence)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_UINT32,
    };

    errno = 0;
    ck_assert_ptr_null(rbh_filter_compare_new(RBH_FOP_IN, RBH_FF_ID, &VALUE));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfcn_regex_without_regex)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_UINT32,
    };

    errno = 0;
    ck_assert_ptr_null(
            rbh_filter_compare_new(RBH_FOP_REGEX, RBH_FF_ID, &VALUE)
            );
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

static const enum rbh_filter_operator BITWISE_OPS[] = {
    RBH_FOP_BITS_ANY_SET,
    RBH_FOP_BITS_ALL_SET,
    RBH_FOP_BITS_ANY_CLEAR,
    RBH_FOP_BITS_ALL_CLEAR,
};

START_TEST(rfcn_bitwise_without_integer)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_STRING,
    };

    errno = 0;
    ck_assert_ptr_null(
            rbh_filter_compare_new(BITWISE_OPS[_i], RBH_FF_ID, &VALUE)
            );
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

/* TODO: test rbh_filter_compare_*_new() */

/*----------------------------------------------------------------------------*
 |                            rbh_filter_and_new()                            |
 *----------------------------------------------------------------------------*/

static const struct rbh_filter COMPARISONS[] = {
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = RBH_FF_ID,
            .value = {
                .type = RBH_VT_BINARY,
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
                .type = RBH_VT_UINT32,
                .uint32 = INT32_MAX,
            },
        },
    },
    {
        .op = RBH_FOP_LOWER_OR_EQUAL,
        .compare = {
            .field = RBH_FF_ATIME,
            .value = {
                .type = RBH_VT_UINT64,
                .uint64 = UINT64_MAX,
            },
        },
    },
    {
        .op = RBH_FOP_GREATER_THAN,
        .compare = {
            .field = RBH_FF_MTIME,
            .value = {
                .type = RBH_VT_INT32,
                .int32 = INT32_MAX,
            }
        },
    },
    {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = RBH_FF_CTIME,
            .value = {
                .type = RBH_VT_INT64,
                .int64 = INT64_MIN,
            },
        },
    },
    {
        .op = RBH_FOP_IN,
        .compare = {
            .field = RBH_FF_TYPE,
            .value = {
                .type = RBH_VT_SEQUENCE,
                .sequence = {
                    .values = NULL,
                    .count = 0,
                },
            },
        },
    },
    {
        .op = RBH_FOP_REGEX,
        .compare = {
            .field = RBH_FF_NAME,
            .value = {
                .type = RBH_VT_REGEX,
                .regex = {
                    .string = "abcdefg",
                    .options = 0,
                },
            },
        },
    },
    {
        .op = RBH_FOP_BITS_ANY_SET,
        .compare = {
            .field = RBH_FF_ID,
            .value = {
                .type = RBH_VT_UINT32,
                .uint32 = UINT32_MAX,
            },
        },
    },
    {
        .op = RBH_FOP_BITS_ALL_SET,
        .compare = {
            .field = RBH_FF_PARENT_ID,
            .value = {
                .type = RBH_VT_UINT64,
                .uint64 = UINT64_MAX,
            },
        },
    },
    {
        .op = RBH_FOP_BITS_ANY_CLEAR,
        .compare = {
            .field = RBH_FF_ATIME,
            .value = {
                .type = RBH_VT_INT32,
                .int32 = INT32_MIN,
            },
        },
    },
    {
        .op = RBH_FOP_BITS_ANY_CLEAR,
        .compare = {
            .field = RBH_FF_ATIME,
            .value = {
                .type = RBH_VT_INT64,
                .int64 = INT64_MIN,
            },
        },
    }
};

START_TEST(rfan_basic)
{
    const struct rbh_filter *filters[ARRAY_SIZE(COMPARISONS) + 1];
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_AND,
        .logical = {
            .filters = filters,
            .count = ARRAY_SIZE(filters),
        },
    };
    struct rbh_filter *filter;

    filters[0] = NULL;
    for (size_t i = 0; i < ARRAY_SIZE(COMPARISONS); i++)
        filters[i + 1] = &COMPARISONS[i];

    filter = rbh_filter_and_new(filters, ARRAY_SIZE(filters));
    ck_assert_ptr_nonnull(filter);

    ck_assert_filter_eq(filter, &FILTER);
    for (size_t i = 0; i < filter->logical.count; i++)
        ck_assert_filter_eq(filter->logical.filters[i], filters[i]);

    free(filter);
}
END_TEST

START_TEST(rfan_zero)
{
    errno = 0;
    ck_assert_ptr_null(rbh_filter_and_new(NULL, 0));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_filter_or_new()                             |
 *----------------------------------------------------------------------------*/

/* The underlying implementation of filter_or() is the same as filter_and()'s.
 * There is no need to test is extensively.
 */
START_TEST(rfon_basic)
{
    const struct rbh_filter * const FILTERS[3] = {};
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_OR,
        .logical = {
            .filters = FILTERS,
            .count = ARRAY_SIZE(FILTERS),
        },
    };
    struct rbh_filter *filter;

    filter = rbh_filter_or_new(FILTERS, ARRAY_SIZE(FILTERS));
    ck_assert_ptr_nonnull(filter);

    ck_assert_filter_eq(filter, &FILTER);
    free(filter);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_filter_not_new()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rfnn_basic)
{
    const struct rbh_filter * const FILTERS[1] = {};
    const struct rbh_filter NEGATED = {
        .op = RBH_FOP_NOT,
        .logical = {
            .filters = FILTERS,
            .count = 1,
        },
    };
    struct rbh_filter *filter;

    filter = rbh_filter_not_new(FILTERS[0]);
    ck_assert_ptr_nonnull(filter);

    ck_assert_filter_eq(filter, &NEGATED);
    free(filter);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("filter interface");

    tests = tcase_create("rbh_filter_compare_new");
    tcase_add_test(tests, rfcn_basic);
    tcase_add_test(tests, rfcn_bad_operator);
    tcase_add_test(tests, rfcn_in_without_sequence);
    tcase_add_test(tests, rfcn_regex_without_regex);
    tcase_add_loop_test(tests, rfcn_bitwise_without_integer, 0,
                        ARRAY_SIZE(BITWISE_OPS));

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_and_new");
    tcase_add_test(tests, rfan_basic);
    tcase_add_test(tests, rfan_zero);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_or_new");
    tcase_add_test(tests, rfon_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_not_new");
    tcase_add_test(tests, rfnn_basic);

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
