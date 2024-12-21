/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <check.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "check-compat.h"
#include "robinhood/plugins/backend.h"

/*----------------------------------------------------------------------------*
 |                     fixtures to run tests in isolation                     |
 *----------------------------------------------------------------------------*/

static const char TMPDIR[] = "/tmp/tmp.d.XXXXXX";
static __thread char tmpdir[sizeof(TMPDIR)];

static void
unchecked_setup_tmpdir(void)
{
    memcpy(tmpdir, TMPDIR, sizeof(tmpdir));
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));
    ck_assert_int_eq(chdir(tmpdir), 0);
}

static int
delete(const char *fpath, const struct stat *sb, int typeflags,
       struct FTW * ftwbuf)
{
    ck_assert_int_eq(remove(fpath), 0);
    (void) sb;
    (void) typeflags;
    (void) ftwbuf;
    return 0;
}

#ifndef NOPENFD
#define NOPENFD (16)
#endif

static void
unchecked_teardown_tmpdir(void)
{
    ck_assert_int_eq(
            nftw(tmpdir, delete, NOPENFD, FTW_DEPTH | FTW_MOUNT | FTW_PHYS), 0
            );
}

/*----------------------------------------------------------------------------*
 |                                posix filter                                |
 *----------------------------------------------------------------------------*/

START_TEST(load_retention)
{
    const struct rbh_plugin_extension *retention;
    const struct rbh_backend_plugin *posix;

    posix = rbh_backend_plugin_import("posix");
    ck_assert_ptr_nonnull(posix);

    retention = rbh_plugin_load_extension(&posix->plugin, "retention");
    ck_assert_ptr_nonnull(retention);
    ck_assert_str_eq(posix->plugin.name, retention->super);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("retention extension");
    tests = tcase_create("load");

    tcase_add_unchecked_fixture(tests, unchecked_setup_tmpdir,
                                unchecked_teardown_tmpdir);
    tcase_add_test(tests, load_retention);
    suite_add_tcase(suite, tests);

    return suite;
}

int
main(void)
{
    int number_failed;
    SRunner *runner;
    Suite *suite;

    suite = unit_suite();
    runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
