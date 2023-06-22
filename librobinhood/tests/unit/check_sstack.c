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

#include "robinhood/sstack.h"

#include "check-compat.h"

/*----------------------------------------------------------------------------*
 |                                 unit tests                                 |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                          rbh_sstack_new()                          |
     *--------------------------------------------------------------------*/

START_TEST(rsn_basic)
{
    struct rbh_sstack *sstack;

    sstack = rbh_sstack_new(0);
    ck_assert_ptr_nonnull(sstack);

    rbh_sstack_destroy(sstack);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                         rbh_sstack_push()                          |
     *--------------------------------------------------------------------*/

START_TEST(rspu_none)
{
    struct rbh_sstack *sstack;

    sstack = rbh_sstack_new(0);
    ck_assert_ptr_nonnull(sstack);

    ck_assert_ptr_nonnull(rbh_sstack_push(sstack, NULL, 0));

    rbh_sstack_destroy(sstack);
}
END_TEST

START_TEST(rspu_more_than_possible)
{
    struct rbh_sstack *sstack;

    sstack = rbh_sstack_new(0);
    ck_assert_ptr_nonnull(sstack);

    errno = 0;
    ck_assert_ptr_null(rbh_sstack_push(sstack, NULL, 1));
    ck_assert_int_eq(errno, EINVAL);

    rbh_sstack_destroy(sstack);
}
END_TEST

START_TEST(rspu_full_twice)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_sstack *sstack;
    void *first, *second;

    sstack = rbh_sstack_new(sizeof(STRING));
    ck_assert_ptr_nonnull(sstack);

    first = rbh_sstack_push(sstack, STRING, sizeof(STRING));
    ck_assert_ptr_nonnull(first);
    ck_assert_ptr_ne(first, STRING);
    ck_assert_mem_eq(first, STRING, sizeof(STRING));

    second = rbh_sstack_push(sstack, STRING, sizeof(STRING));
    ck_assert_ptr_nonnull(second);
    ck_assert_ptr_ne(second, STRING);
    ck_assert_ptr_ne(first, second);
    ck_assert_mem_eq(second, STRING, sizeof(STRING));

    rbh_sstack_destroy(sstack);
}
END_TEST

START_TEST(rspu_reuse_stacks)
{
    struct rbh_sstack *sstack;
    void *data;
    size_t _;

    sstack = rbh_sstack_new(1);
    ck_assert_ptr_nonnull(sstack);

    ck_assert_ptr_nonnull(rbh_sstack_push(sstack, NULL, 1));

    data = rbh_sstack_push(sstack, NULL, 1);
    ck_assert_ptr_nonnull(data);

    ck_assert_int_eq(rbh_sstack_pop(sstack, 1), 0);
    ck_assert_ptr_ne(rbh_sstack_peek(sstack, &_), data);

    ck_assert_ptr_eq(rbh_sstack_push(sstack, NULL, 1), data);

    rbh_sstack_destroy(sstack);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                         rbh_sstack_peek()                          |
     *--------------------------------------------------------------------*/

START_TEST(rspe_full)
{
    struct rbh_sstack *sstack;
    size_t readable;
    void *data;

    sstack = rbh_sstack_new(1);
    ck_assert_ptr_nonnull(sstack);

    data = rbh_sstack_push(sstack, NULL, 1);
    ck_assert_ptr_nonnull(data);

    ck_assert_ptr_eq(rbh_sstack_peek(sstack, &readable), data);
    ck_assert_uint_eq(readable, 1);

    rbh_sstack_destroy(sstack);
}
END_TEST

START_TEST(rspe_full_twice)
{
    struct rbh_sstack *sstack;
    void *first, *second;
    size_t readable;

    sstack = rbh_sstack_new(2);
    ck_assert_ptr_nonnull(sstack);

    first = rbh_sstack_push(sstack, NULL, 2);
    ck_assert_ptr_nonnull(first);
    second = rbh_sstack_push(sstack, NULL, 1);
    ck_assert_ptr_nonnull(second);

    ck_assert_ptr_eq(rbh_sstack_peek(sstack, &readable), second);
    ck_assert_uint_eq(readable, 1);
    ck_assert_int_eq(rbh_sstack_pop(sstack, readable), 0);

    ck_assert_ptr_eq(rbh_sstack_peek(sstack, &readable), first);
    ck_assert_uint_eq(readable, 2);

    rbh_sstack_destroy(sstack);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_sstack_pop()                          |
     *--------------------------------------------------------------------*/

START_TEST(rspo_too_much)
{
    struct rbh_sstack *sstack;

    sstack = rbh_sstack_new(0);
    ck_assert_ptr_nonnull(sstack);

    errno = 0;
    ck_assert_int_eq(rbh_sstack_pop(sstack, 1), -1);
    ck_assert_int_eq(errno, EINVAL);

    rbh_sstack_destroy(sstack);
}
END_TEST

START_TEST(rspo_after_full_twice)
{
    struct rbh_sstack *sstack;

    sstack = rbh_sstack_new(2);
    ck_assert_ptr_nonnull(sstack);

    ck_assert_ptr_nonnull(rbh_sstack_push(sstack, NULL, 1));
    ck_assert_ptr_nonnull(rbh_sstack_push(sstack, NULL, 2));

    ck_assert_int_eq(rbh_sstack_pop(sstack, 2), 0);

    errno = 0;
    ck_assert_int_eq(rbh_sstack_pop(sstack, 2), -1);
    ck_assert_int_eq(errno, EINVAL);

    ck_assert_int_eq(rbh_sstack_pop(sstack, 1), 0);

    rbh_sstack_destroy(sstack);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                        rbh_sstack_shrink()                         |
     *--------------------------------------------------------------------*/

START_TEST(rss_basic)
{
    struct rbh_sstack *sstack;

    sstack = rbh_sstack_new(1);
    ck_assert_ptr_nonnull(sstack);

    ck_assert_ptr_nonnull(rbh_sstack_push(sstack, NULL, 1));
    ck_assert_ptr_nonnull(rbh_sstack_push(sstack, NULL, 1));
    ck_assert_int_eq(rbh_sstack_pop(sstack, 1), 0);

    rbh_sstack_shrink(sstack);

    rbh_sstack_destroy(sstack);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("unit tests");

    tests = tcase_create("rbh_sstack_new()");
    tcase_add_test(tests, rsn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_sstack_push()");
    tcase_add_test(tests, rspu_none);
    tcase_add_test(tests, rspu_more_than_possible);
    tcase_add_test(tests, rspu_full_twice);
    tcase_add_test(tests, rspu_reuse_stacks);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_sstack_peek()");
    tcase_add_test(tests, rspe_full);
    tcase_add_test(tests, rspe_full_twice);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_sstack_pop()");
    tcase_add_test(tests, rspo_too_much);
    tcase_add_test(tests, rspo_after_full_twice);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_sstack_shrink()");
    tcase_add_test(tests, rss_basic);

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
