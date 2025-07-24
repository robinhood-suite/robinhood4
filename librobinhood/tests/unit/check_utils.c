/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check-compat.h"
#include "robinhood/utils.h"

/*----------------------------------------------------------------------------*
 |                                 command_call                               |
 *----------------------------------------------------------------------------*/

static int
parse_line(void *arg, char *line, size_t size, int stream)
{
    GList **ctx = (GList **)arg;
    int len;

    if (line == NULL)
        return -EINVAL;

    len = strnlen(line, size);
    /* terminate the string */
    if (len >= size)
        line[len - 1] = '\0';

    *ctx = g_list_append(*ctx, g_strdup(line));
    return 0;
}

START_TEST(ccs)
{
    GList *lines = NULL;
    char buffer[256];
    GList *line;
    FILE *hosts;
    int rc = 0;

    hosts = fopen("/etc/hosts", "r");
    ck_assert_ptr_nonnull(hosts);

    /** call a command and call cb_func for each output line */
    rc = command_call("cat /etc/hosts", parse_line, &lines);

    ck_assert_int_eq(rc, 0);

    line = lines;
    while (fgets(buffer, sizeof(buffer), hosts)) {
        size_t buffer_length = strlen(buffer);

        if (buffer[buffer_length - 1] == '\n')
            buffer[buffer_length - 1] = '\0';

        ck_assert_str_eq(buffer, (char *) line->data);
        line = line->next;
    }

    fclose(hosts);

    g_list_free_full(lines, free);
}
END_TEST

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

    tests = tcase_create("command_call");
    tcase_add_test(tests, ccs);

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
