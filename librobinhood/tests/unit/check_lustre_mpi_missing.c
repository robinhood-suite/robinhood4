/* This file is part of the RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "check-compat.h"
#include "robinhood/backends/lustre_mpi.h"

/*----------------------------------------------------------------------------*
 |                     fixtures to run tests in isolation                     |
 *----------------------------------------------------------------------------*/

static const char TMPDIR[] = "/mnt/lustre/tmp.d.XXXXXX";
static __thread char *tmpdir;

static void
unchecked_setup_tmpdir(void)
{
    const char *lustre_tmpdir = getenv("LUSTRE_TMPDIR");

    if (lustre_tmpdir == NULL)
        lustre_tmpdir = TMPDIR;

    tmpdir = strdup(lustre_tmpdir);

    if (access(tmpdir, W_OK | X_OK)) {
        ck_assert_ptr_nonnull(mkdtemp(tmpdir));
        ck_assert_int_eq(chdir(tmpdir), 0);
    }
}

static int
delete(const char *fpath, const struct stat *sb, int typeflags,
       struct FTW * ftwbuf)
{
    ck_assert_int_eq(remove(fpath), 0);
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
 |                           lustre mpi filter                                |
 *----------------------------------------------------------------------------*/

START_TEST(lf_missing_root)
{
    const struct rbh_filter_options OPTIONS = {};
    struct rbh_backend *lustre_mpi;

    lustre_mpi = rbh_lustre_mpi_backend_new("missing");
    ck_assert_ptr_nonnull(lustre_mpi);

    errno = 0;
    ck_assert_ptr_null(rbh_backend_filter(lustre_mpi, NULL, &OPTIONS));
    ck_assert_int_eq(errno, ENOENT);

    rbh_backend_destroy(lustre_mpi);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("lustre mpi backend");
    tests = tcase_create("filter");
    tcase_add_unchecked_fixture(tests, unchecked_setup_tmpdir,
                                unchecked_teardown_tmpdir);
    tcase_add_test(tests, lf_missing_root);

    suite_add_tcase(suite, tests);

    return suite;
}

int
main(int argc, char **argv)
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
