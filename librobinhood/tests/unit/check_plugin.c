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

#include "check-compat.h"
#include "robinhood/backend.h"
#include "robinhood/plugin.h"

/*----------------------------------------------------------------------------*
 |                          Robinhood Plugin Version                          |
 *----------------------------------------------------------------------------*/

START_TEST(rpv_major)
{
    ck_assert_uint_eq(RPV_MAJOR(RPV(1, 2, 3)), 1);
}
END_TEST

START_TEST(rpv_minor)
{
    ck_assert_uint_eq(RPV_MINOR(RPV(1, 2, 3)), 2);
}
END_TEST

START_TEST(rpv_revision)
{
    ck_assert_uint_eq(RPV_REVISION(RPV(1, 2, 3)), 3);
}
END_TEST

START_TEST(rpv_limits)
{
    ck_assert_uint_eq(UINT64_MAX, RPV(0x3ff, 0x7ffffff, 0x7ffffff));
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_plugin_import()                             |
 *----------------------------------------------------------------------------*/

struct rbh_backend_plugin;
struct rbh_config;

typedef struct rbh_backend *(*new_t)(const struct rbh_backend_plugin *,
                                     const char *,
                                     const char *,
                                     struct rbh_config *config,
                                     bool read_only);

START_TEST(rbi_posix)
{
    new_t rbh_posix_backend_new;
    struct rbh_backend *posix;

    rbh_posix_backend_new = rbh_plugin_import("posix", "rbh_posix_backend_new");
    ck_assert_ptr_nonnull(rbh_posix_backend_new);

    posix = rbh_posix_backend_new(NULL, NULL, "", NULL, true);
    ck_assert_ptr_nonnull(posix);

    rbh_backend_destroy(posix);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("plugin");
    tests = tcase_create("version");
    tcase_add_test(tests, rpv_major);
    tcase_add_test(tests, rpv_minor);
    tcase_add_test(tests, rpv_revision);
    tcase_add_test(tests, rpv_limits);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_plugin_import");
    tcase_add_test(tests, rbi_posix);

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
