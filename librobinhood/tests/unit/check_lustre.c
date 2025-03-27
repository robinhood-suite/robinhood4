/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
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
#include "robinhood/backends/lustre.h"

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
 |                               lustre filter                                |
 *----------------------------------------------------------------------------*/

START_TEST(lf_missing_root)
{
    const struct rbh_filter_options OPTIONS = {};
    const struct rbh_backend_plugin *posix;
    struct rbh_backend *lustre;

    posix = rbh_backend_plugin_import("posix");
    ck_assert_ptr_nonnull(posix);
    lustre = rbh_backend_plugin_new(posix, "lustre", "missing", NULL, false);
    ck_assert_ptr_nonnull(lustre);

    errno = 0;
    ck_assert_ptr_null(rbh_backend_filter(lustre, NULL, &OPTIONS, NULL));
    ck_assert_int_eq(errno, ENOENT);

    rbh_backend_destroy(lustre);
}
END_TEST

START_TEST(lf_empty_root)
{
    const struct rbh_filter_options OPTIONS = { 0 };
    const struct rbh_filter_output OUTPUT = {
        .projection = {
            .fsentry_mask = RBH_FP_PARENT_ID,
        },
    };
    const struct rbh_backend_plugin *posix;
    struct rbh_mut_iterator *fsentries;
    static const char *EMPTY = "empty";
    struct rbh_fsentry *fsentry;
    struct rbh_backend *lustre;

    ck_assert_int_eq(mkdir(EMPTY, S_IRWXU), 0);

    posix = rbh_backend_plugin_import("posix");
    ck_assert_ptr_nonnull(posix);
    lustre = rbh_backend_plugin_new(posix, "lustre", EMPTY, NULL, false);
    ck_assert_ptr_nonnull(lustre);

    fsentries = rbh_backend_filter(lustre, NULL, &OPTIONS, &OUTPUT);
    ck_assert_ptr_nonnull(fsentries);

    fsentry = rbh_mut_iter_next(fsentries);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert(fsentry->mask & RBH_FP_PARENT_ID);
    ck_assert_int_eq(fsentry->parent_id.size, 0);

    free(fsentry);

    errno = 0;
    ck_assert_ptr_null(rbh_mut_iter_next(fsentries));
    ck_assert_int_eq(errno, ENODATA);

    rbh_mut_iter_destroy(fsentries);
    rbh_backend_destroy(lustre);
    ck_assert_int_eq(rmdir(EMPTY), 0);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("lustre backend");
    tests = tcase_create("filter");
    tcase_add_unchecked_fixture(tests, unchecked_setup_tmpdir,
                                unchecked_teardown_tmpdir);
    tcase_add_test(tests, lf_missing_root);
    tcase_add_test(tests, lf_empty_root);

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
