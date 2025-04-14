/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <signal.h>

#include "check-compat.h"
#include "robinhood/backend.h"

static const struct rbh_backend_operations TEST_BACKEND_OPS = {
    .destroy = free,
};

static const struct rbh_backend TEST_BACKEND = {
    .id = UINT8_MAX,
    .ops = &TEST_BACKEND_OPS,
};

static struct rbh_backend *
test_backend_new(void)
{
    struct rbh_backend *backend;

    backend = malloc(sizeof(*backend));
    ck_assert_ptr_nonnull(backend);

    *backend = TEST_BACKEND;
    return backend;
}

/*----------------------------------------------------------------------------*
 |                           rbh_backend_get_option                           |
 *----------------------------------------------------------------------------*/

START_TEST(rbgo_unsupported)
{
    struct rbh_backend *backend = test_backend_new();

    ck_assert_int_eq(rbh_backend_get_option(backend, RBH_BO_FIRST(backend->id),
                                            NULL, NULL), -1);
    ck_assert_int_eq(errno, ENOTSUP);

    rbh_backend_destroy(backend);
}
END_TEST

START_TEST(rbgo_wrong_option)
{
    struct rbh_backend *backend = test_backend_new();

    ck_assert_int_eq(rbh_backend_get_option(
                backend, RBH_BO_FIRST(backend->id) - 1, NULL, NULL
                ), -1);
    ck_assert_int_eq(errno, EINVAL);

    rbh_backend_destroy(backend);
}
END_TEST

START_TEST(rbgo_generic_deprecated)
{
    struct rbh_backend *backend = test_backend_new();

    ck_assert_int_eq(rbh_backend_get_option(backend, RBH_GBO_DEPRECATED, NULL,
                                            NULL), -1);
    ck_assert_int_eq(errno, ENOTSUP);

    rbh_backend_destroy(backend);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_backend_set_option                           |
 *----------------------------------------------------------------------------*/

START_TEST(rbso_unsupported)
{
    struct rbh_backend *backend = test_backend_new();

    ck_assert_int_eq(rbh_backend_set_option(backend, RBH_BO_FIRST(backend->id),
                                            NULL, 0), -1);
    ck_assert_int_eq(errno, ENOTSUP);

    rbh_backend_destroy(backend);
}
END_TEST

START_TEST(rbso_wrong_option)
{
    struct rbh_backend *backend = test_backend_new();

    ck_assert_int_eq(rbh_backend_get_option(
                backend, RBH_BO_FIRST(backend->id) - 1, NULL, NULL
                ), -1);
    ck_assert_int_eq(errno, EINVAL);

    rbh_backend_destroy(backend);
}
END_TEST

START_TEST(rbso_generic_deprecated)
{
    struct rbh_backend *backend = test_backend_new();

    ck_assert_int_eq(rbh_backend_set_option(backend, RBH_GBO_DEPRECATED, NULL,
                                            0), -1);
    ck_assert_int_eq(errno, ENOTSUP);

    rbh_backend_destroy(backend);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                             rbh_backend_update                             |
 *----------------------------------------------------------------------------*/

START_TEST(rbu_unsupported)
{
    struct rbh_backend *backend = test_backend_new();

    ck_assert_int_eq(rbh_backend_update(backend, NULL), -1);
    ck_assert_int_eq(errno, ENOTSUP);

    rbh_backend_destroy(backend);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                             rbh_backend_filter                             |
 *----------------------------------------------------------------------------*/

START_TEST(rbf_unsupported)
{
    struct rbh_backend *backend = test_backend_new();
    struct rbh_filter_options options = {};

    ck_assert_ptr_null(rbh_backend_filter(backend, NULL, &options, NULL, NULL));
    ck_assert_int_eq(errno, ENOTSUP);

    rbh_backend_destroy(backend);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                             rbh_backend_branch                             |
 *----------------------------------------------------------------------------*/

START_TEST(rbb_unsupported)
{
    struct rbh_backend *backend = test_backend_new();

    ck_assert_ptr_null(rbh_backend_branch(backend, NULL, NULL));
    ck_assert_int_eq(errno, ENOTSUP);

    rbh_backend_destroy(backend);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("backend");

    tests = tcase_create("unsupported operations");
    tcase_add_test(tests, rbgo_unsupported);
    tcase_add_test(tests, rbso_unsupported);

    suite_add_tcase(suite, tests);

    tests = tcase_create("unsupported fsentries operations");
    tcase_add_test(tests, rbu_unsupported);
    tcase_add_test(tests, rbf_unsupported);
    tcase_add_test(tests, rbb_unsupported);

    suite_add_tcase(suite, tests);

    tests = tcase_create("options");
    tcase_add_test(tests, rbgo_wrong_option);
    tcase_add_test(tests, rbgo_generic_deprecated);
    tcase_add_test(tests, rbso_wrong_option);
    tcase_add_test(tests, rbso_generic_deprecated);

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
