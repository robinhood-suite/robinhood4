/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>

#include "robinhood/stack.h"

#include "check-compat.h"

/*----------------------------------------------------------------------------*
 |                              rbh_stack_new()                               |
 *----------------------------------------------------------------------------*/

START_TEST(rsn_basic)
{
    struct rbh_stack *stack;

    stack = rbh_stack_new(0);
    ck_assert_ptr_nonnull(stack);

    rbh_stack_destroy(stack);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              rbh_stack_push()                              |
 *----------------------------------------------------------------------------*/

START_TEST(rspu_more_than_possible)
{
    struct rbh_stack *stack;

    stack = rbh_stack_new(0);
    ck_assert_ptr_nonnull(stack);

    errno = 0;
    ck_assert_ptr_null(rbh_stack_push(stack, NULL, 1));
    ck_assert_int_eq(errno, EINVAL);

    rbh_stack_destroy(stack);
}
END_TEST

START_TEST(rspu_more_than_available)
{
    struct rbh_stack *stack;

    stack = rbh_stack_new(2);
    ck_assert_ptr_nonnull(stack);

    ck_assert_ptr_nonnull(rbh_stack_push(stack, NULL, 1));

    errno = 0;
    ck_assert_ptr_null(rbh_stack_push(stack, NULL, 2));
    ck_assert_int_eq(errno, ENOBUFS);

    rbh_stack_destroy(stack);
}
END_TEST

START_TEST(rspu_null)
{
    struct rbh_stack *stack;
    size_t size;
    void *data;

    stack = rbh_stack_new(8);
    ck_assert_ptr_nonnull(stack);

    data = rbh_stack_push(stack, NULL, 1);
    ck_assert_ptr_nonnull(data);

    rbh_stack_peek(stack, &size);
    ck_assert_uint_eq(size, 1);

    rbh_stack_destroy(stack);
}
END_TEST

START_TEST(rspu_full)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_stack *stack;
    void *data;

    stack = rbh_stack_new(sizeof(STRING));
    ck_assert_ptr_nonnull(stack);

    data = rbh_stack_push(stack, STRING, sizeof(STRING));
    ck_assert_ptr_nonnull(data);
    ck_assert_mem_eq(data, STRING, sizeof(STRING));

    ck_assert_ptr_null(rbh_stack_push(stack, NULL, 1));

    rbh_stack_destroy(stack);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              rbh_stack_peek()                              |
 *----------------------------------------------------------------------------*/

START_TEST(rspe_empty)
{
    struct rbh_stack *stack;
    size_t size;

    stack = rbh_stack_new(0);
    ck_assert_ptr_nonnull(stack);

    ck_assert_ptr_nonnull(rbh_stack_peek(stack, &size));
    ck_assert_uint_eq(size, 0);

    rbh_stack_destroy(stack);
}
END_TEST

START_TEST(rspe_some)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_stack *stack;
    size_t size;
    void *data;

    stack = rbh_stack_new(sizeof(STRING));
    ck_assert_ptr_nonnull(stack);

    data = rbh_stack_push(stack, STRING, sizeof(STRING));
    ck_assert_ptr_nonnull(data);

    ck_assert_ptr_eq(rbh_stack_peek(stack, &size), data);
    ck_assert_uint_eq(size, sizeof(STRING));

    rbh_stack_destroy(stack);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              rbh_stack_pop()                               |
 *----------------------------------------------------------------------------*/

START_TEST(rspo_full)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_stack *stack;

    stack = rbh_stack_new(sizeof(STRING));
    ck_assert_ptr_nonnull(stack);

    ck_assert_ptr_nonnull(rbh_stack_push(stack, STRING, sizeof(STRING)));

    ck_assert_int_eq(rbh_stack_pop(stack, sizeof(STRING)), 0);

    rbh_stack_destroy(stack);
}
END_TEST

START_TEST(rspo_too_much)
{
    struct rbh_stack *stack;

    stack = rbh_stack_new(0);
    ck_assert_ptr_nonnull(stack);

    errno = 0;
    ck_assert_int_eq(rbh_stack_pop(stack, 1), -1);
    ck_assert_int_eq(errno, EINVAL);

    rbh_stack_destroy(stack);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("unit tests");

    tests = tcase_create("rbh_stack_new");
    tcase_add_test(tests, rsn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_stack_push");
    tcase_add_test(tests, rspu_more_than_possible);
    tcase_add_test(tests, rspu_more_than_available);
    tcase_add_test(tests, rspu_null);
    tcase_add_test(tests, rspu_full);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_stack_peek");
    tcase_add_test(tests, rspe_empty);
    tcase_add_test(tests, rspe_some);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_stack_pop");
    tcase_add_test(tests, rspo_full);
    tcase_add_test(tests, rspo_too_much);

    suite_add_tcase(suite, tests);

    return suite;
}

/*----------------------------------------------------------------------------*
 |                                integration                                 |
 *----------------------------------------------------------------------------*/

START_TEST(one_by_one)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_stack *stack;
    size_t size;
    void *data;

    stack = rbh_stack_new(sizeof(STRING));
    ck_assert_ptr_nonnull(stack);

    for (size_t i = 0; i < sizeof(STRING); i++)
        rbh_stack_push(stack, &STRING[sizeof(STRING) - 1 - i], 1);

    data = rbh_stack_peek(stack, &size);
    ck_assert_ptr_nonnull(data);
    ck_assert_uint_eq(size, sizeof(STRING));

    ck_assert_mem_eq(data, STRING, sizeof(STRING));

    rbh_stack_destroy(stack);
}
END_TEST

static Suite *
integration_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("integration tests");

    tests = tcase_create("examples");
    tcase_add_test(tests, one_by_one);

    suite_add_tcase(suite, tests);

    return suite;
}

int
main(void)
{
    int number_failed;
    SRunner *runner;

    runner = srunner_create(unit_suite());
    srunner_add_suite(runner, integration_suite());

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
