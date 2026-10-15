/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "check-compat.h"
#include "robinhood/config.h"

#include "timeout.h"

#define CONFIG_FILE "check_timeout.yaml"

static void
load_config(const char *content)
{
    FILE *file;

    file = fopen(CONFIG_FILE, "w");
    ck_assert_ptr_nonnull(file);
    ck_assert(fputs(content, file) != EOF);
    ck_assert(fclose(file) == 0);

    rbh_config_free();
    ck_assert_int_eq(rbh_config_load_from_path(CONFIG_FILE), 0);
}

static void
teardown(void)
{
    rbh_config_free();
    unlink(CONFIG_FILE);
}

START_TEST(st_defaults)
{
    load_config("---\nfoo: \"bar\"\n");

    ck_assert_int_eq(mongo_socket_timeout_from_config(true), INT32_MAX);
    ck_assert_int_eq(mongo_socket_timeout_from_config(false), 600000);
}
END_TEST

START_TEST(st_read_explicit)
{
    load_config("---\nmongo:\n    read_socket_timeout: !int32 7\n");

    ck_assert_int_eq(mongo_socket_timeout_from_config(true), 7);
    ck_assert_int_eq(mongo_socket_timeout_from_config(false), 600000);
}
END_TEST

/* A configured timeout of exactly 1 ms is a legal value, not a KPR_NOT_FOUND
 * sentinel (enum value 1).
 */
START_TEST(st_one_ms)
{
    load_config("---\nmongo:\n    read_socket_timeout: !int32 1\n"
                "    write_socket_timeout: !int32 1\n");

    ck_assert_int_eq(mongo_socket_timeout_from_config(true), 1);
    ck_assert_int_eq(mongo_socket_timeout_from_config(false), 1);
}
END_TEST

START_TEST(st_zero_is_unlimited)
{
    load_config("---\nmongo:\n    read_socket_timeout: !int32 0\n"
                "    write_socket_timeout: !int32 0\n");

    ck_assert_int_eq(mongo_socket_timeout_from_config(true), INT32_MAX);
    ck_assert_int_eq(mongo_socket_timeout_from_config(false), INT32_MAX);
}
END_TEST

START_TEST(st_negative)
{
    load_config("---\nmongo:\n    read_socket_timeout: !int32 -5\n");

    errno = 0;
    ck_assert_int_eq(mongo_socket_timeout_from_config(true), -1);
    ck_assert_int_eq(errno, EINVAL);

    load_config("---\nmongo:\n    write_socket_timeout: !int32 -7\n");

    errno = 0;
    ck_assert_int_eq(mongo_socket_timeout_from_config(false), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(st_deprecated_cursor_timeout)
{
    load_config("---\nmongo:\n    cursor_timeout: !int32 30000\n");

    ck_assert_int_eq(mongo_socket_timeout_from_config(true), 30000);
    ck_assert_int_eq(mongo_socket_timeout_from_config(false), 30000);

    load_config("---\nmongo:\n    cursor_timeout: !int32 0\n");

    ck_assert_int_eq(mongo_socket_timeout_from_config(true), INT32_MAX);
    ck_assert_int_eq(mongo_socket_timeout_from_config(false), INT32_MAX);

    load_config("---\nmongo:\n    cursor_timeout: !int32 -3\n");

    errno = 0;
    ck_assert_int_eq(mongo_socket_timeout_from_config(false), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(st_mode_specific_key_wins)
{
    load_config("---\nmongo:\n    read_socket_timeout: !int32 5\n"
                "    write_socket_timeout: !int32 6\n"
                "    cursor_timeout: !int32 999\n");

    ck_assert_int_eq(mongo_socket_timeout_from_config(true), 5);
    ck_assert_int_eq(mongo_socket_timeout_from_config(false), 6);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("timeout");

    tests = tcase_create("socket_timeout");
    tcase_add_unchecked_fixture(tests, NULL, teardown);
    tcase_add_test(tests, st_defaults);
    tcase_add_test(tests, st_read_explicit);
    tcase_add_test(tests, st_one_ms);
    tcase_add_test(tests, st_zero_is_unlimited);
    tcase_add_test(tests, st_negative);
    tcase_add_test(tests, st_deprecated_cursor_timeout);
    tcase_add_test(tests, st_mode_specific_key_wins);

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
