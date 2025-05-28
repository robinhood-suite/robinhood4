/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "check-compat.h"
#include "robinhood/utils.h"

/*----------------------------------------------------------------------------*
 |                              size_printer                                  |
 *----------------------------------------------------------------------------*/

START_TEST(sp_b)
{
    char _buffer[16];
    size_t bufsize;
    char *buffer;
    size_t size;

    size = 1 * 4;
    buffer = _buffer;
    bufsize = sizeof(_buffer);

    size_printer(buffer, bufsize, size);

    ck_assert_str_eq(buffer, "4 Bytes");
}
END_TEST

START_TEST(sp_kb)
{
    char _buffer[16];
    size_t bufsize;
    char *buffer;
    size_t size;

    size = (1 << 10) * 3;
    buffer = _buffer;
    bufsize = sizeof(_buffer);

    size_printer(buffer, bufsize, size);

    ck_assert_str_eq(buffer, "3.00 KB");
}
END_TEST

START_TEST(sp_mb)
{
    char _buffer[16];
    size_t bufsize;
    char *buffer;
    size_t size;

    size = 1 << 20;
    buffer = _buffer;
    bufsize = sizeof(_buffer);

    size_printer(buffer, bufsize, size);

    ck_assert_str_eq(buffer, "1.00 MB");
}
END_TEST

START_TEST(sp_gb)
{
    char _buffer[16];
    size_t bufsize;
    char *buffer;
    size_t size;

    size = (1LL << 30) * 55;
    buffer = _buffer;
    bufsize = sizeof(_buffer);

    size_printer(buffer, bufsize, size);

    ck_assert_str_eq(buffer, "55.00 GB");
}
END_TEST

START_TEST(sp_tb)
{
    char _buffer[16];
    size_t bufsize;
    char *buffer;
    size_t size;

    size = (1LL << 40) * 7;
    buffer = _buffer;
    bufsize = sizeof(_buffer);

    size_printer(buffer, bufsize, size);

    ck_assert_str_eq(buffer, "7.00 TB");
}
END_TEST

START_TEST(sp_pb)
{
    char _buffer[16];
    size_t bufsize;
    char *buffer;
    size_t size;

    size = (1LL << 50) * 9;
    buffer = _buffer;
    bufsize = sizeof(_buffer);

    size_printer(buffer, bufsize, size);

    ck_assert_str_eq(buffer, "9.00 PB");
}
END_TEST

START_TEST(sp_eb)
{
    char _buffer[16];
    size_t bufsize;
    char *buffer;
    size_t size;

    size = (1LL << 60) * 2;
    buffer = _buffer;
    bufsize = sizeof(_buffer);

    size_printer(buffer, bufsize, size);

    ck_assert_str_eq(buffer, "2.00 EB");
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              difftime_printer                              |
 *----------------------------------------------------------------------------*/

START_TEST(dp_s)
{
    char _buffer[16];
    size_t bufsize;
    char *buffer;

    buffer = _buffer;
    bufsize = sizeof(_buffer);
    difftime_printer(buffer, bufsize, 42);

    ck_assert_str_eq(buffer, "42s");
}
END_TEST

START_TEST(dp_m)
{
    char _buffer[16];
    size_t bufsize;
    char *buffer;

    buffer = _buffer;
    bufsize = sizeof(_buffer);
    difftime_printer(buffer, bufsize, 123);

    ck_assert_str_eq(buffer, "2m3s");
}
END_TEST

START_TEST(dp_h)
{
    char _buffer[16];
    size_t bufsize;
    char *buffer;

    buffer = _buffer;
    bufsize = sizeof(_buffer);
    difftime_printer(buffer, bufsize, 3671);

    ck_assert_str_eq(buffer, "1h1m11s");
}
END_TEST

START_TEST(dp_d)
{
    char _buffer[16];
    size_t bufsize;
    char *buffer;

    buffer = _buffer;
    bufsize = sizeof(_buffer);
    difftime_printer(buffer, bufsize, 86400 * 3 + 3600 * 8 + 60 * 36 + 48);

    ck_assert_str_eq(buffer, "3d8h36m48s");
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("utils");

    tests = tcase_create("size_printer");
    tcase_add_test(tests, sp_b);
    tcase_add_test(tests, sp_kb);
    tcase_add_test(tests, sp_mb);
    tcase_add_test(tests, sp_gb);
    tcase_add_test(tests, sp_tb);
    tcase_add_test(tests, sp_pb);
    tcase_add_test(tests, sp_eb);

    suite_add_tcase(suite, tests);

    tests = tcase_create("difftime_printer");
    tcase_add_test(tests, dp_s);
    tcase_add_test(tests, dp_m);
    tcase_add_test(tests, dp_h);
    tcase_add_test(tests, dp_d);

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
