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
#include "robinhood/backends/posix.h"
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

/*----------------------------------------------------------------------------*
 |                     fixtures to run tests in isolation                     |
 *----------------------------------------------------------------------------*/

static const char TMPDIR[] = "tmp.d.XXXXXX";
static __thread char tmpdir[PATH_MAX];

static void
unchecked_setup_tmpdir(void)
{
    char *env_tmpdir = getenv("TMPDIR");

    if (env_tmpdir == NULL)
        env_tmpdir = "/tmp";

    snprintf(tmpdir, PATH_MAX, "%s/%s", env_tmpdir, TMPDIR);
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

START_TEST(pf_missing_root)
{
    const struct rbh_filter_options OPTIONS = {};
    struct rbh_backend *posix;

    posix = rbh_posix_backend_new(NULL, NULL, "missing", NULL);
    ck_assert_ptr_nonnull(posix);

    errno = 0;
    ck_assert_ptr_null(rbh_backend_filter(posix, NULL, &OPTIONS, NULL));
    ck_assert_int_eq(errno, ENOENT);

    rbh_backend_destroy(posix);
}
END_TEST

START_TEST(pf_empty_root)
{
    const struct rbh_filter_options OPTIONS = { 0 };
    const struct rbh_filter_output OUTPUT = {
        .projection = {
            .fsentry_mask = RBH_FP_PARENT_ID,
        },
    };
    static const char *EMPTY = "empty";
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *fsentry;
    struct rbh_backend *posix;


    ck_assert_int_eq(mkdir(EMPTY, S_IRWXU), 0);

    posix = rbh_posix_backend_new(NULL, NULL, EMPTY, NULL);
    ck_assert_ptr_nonnull(posix);

    fsentries = rbh_backend_filter(posix, NULL, &OPTIONS, &OUTPUT);
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
    rbh_backend_destroy(posix);
    ck_assert_int_eq(rmdir(EMPTY), 0);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                               posix options                                |
 *----------------------------------------------------------------------------*/

static const unsigned int PBO_MAX = RBH_PBO_STATX_SYNC_TYPE + 1;

START_TEST(pbo_get_unknown)
{
    struct rbh_backend *posix;

    posix = rbh_posix_backend_new(NULL, NULL, "", NULL);
    ck_assert_ptr_nonnull(posix);

    ck_assert_int_eq(rbh_backend_get_option(posix, PBO_MAX, NULL, NULL),
                     -1);
    ck_assert_int_eq(errno, ENOPROTOOPT);

    rbh_backend_destroy(posix);
}
END_TEST

START_TEST(pbo_set_unknown)
{
    struct rbh_backend *posix;

    posix = rbh_posix_backend_new(NULL, NULL, "", NULL);
    ck_assert_ptr_nonnull(posix);

    ck_assert_int_eq(rbh_backend_set_option(posix, PBO_MAX, NULL, 0), -1);
    ck_assert_int_eq(errno, ENOPROTOOPT);

    rbh_backend_destroy(posix);
}
END_TEST

#define BO_INDEX(option) ((option) & UINT8_MAX)

static const size_t PBO_SIZES[] = {
    [BO_INDEX(RBH_PBO_STATX_SYNC_TYPE)] = sizeof(int),
};

START_TEST(pbo_get_sizes)
{
    size_t size = PBO_SIZES[BO_INDEX(_i)];
    struct rbh_backend *posix;
    void *data;

    posix = rbh_posix_backend_new(NULL, NULL, "", NULL);
    ck_assert_ptr_nonnull(posix);

    data = malloc(size + 1);
    ck_assert_ptr_nonnull(data);

    /* Too little */
    size -= 1;
    ck_assert_int_eq(rbh_backend_get_option(posix, _i, data, &size), -1);
    ck_assert_int_eq(errno, EOVERFLOW);
    ck_assert_uint_eq(size, PBO_SIZES[BO_INDEX(_i)]);

    /* Too much */
    size += 1;
    ck_assert_int_eq(rbh_backend_get_option(posix, _i, data, &size), 0);
    ck_assert_uint_eq(size, PBO_SIZES[BO_INDEX(_i)]);

    free(data);

    rbh_backend_destroy(posix);
}
END_TEST

static const int PSST_DEFAULT = AT_STATX_SYNC_AS_STAT;

static const void *PBO_DEFAULTS[] = {
    [BO_INDEX(RBH_PBO_STATX_SYNC_TYPE)] = &PSST_DEFAULT,
};

START_TEST(pbo_defaults)
{
    struct rbh_backend *posix;
    size_t size;
    void *data;

    posix = rbh_posix_backend_new(NULL, NULL, "", NULL);
    ck_assert_ptr_nonnull(posix);

    size = PBO_SIZES[BO_INDEX(_i)];
    data = malloc(size);
    ck_assert_ptr_nonnull(data);

    ck_assert_int_eq(rbh_backend_get_option(posix, _i, data, &size), 0);
    ck_assert_uint_eq(size, PBO_SIZES[BO_INDEX(_i)]);
    ck_assert_mem_eq(data, PBO_DEFAULTS[BO_INDEX(_i)], size);

    free(data);

    rbh_backend_destroy(posix);
}
END_TEST

START_TEST(pbo_set_sizes)
{
    size_t size = PBO_SIZES[BO_INDEX(_i)];
    struct rbh_backend *posix;

    posix = rbh_posix_backend_new(NULL, NULL, "", NULL);
    ck_assert_ptr_nonnull(posix);

    /* Too little */
    size -= 1;
    ck_assert_int_eq(rbh_backend_set_option(posix, _i, NULL, size), -1);
    ck_assert_int_eq(errno, EINVAL);

    /* Too much */
    size += 2;
    ck_assert_int_eq(rbh_backend_set_option(posix, _i, NULL, size), -1);
    ck_assert_int_eq(errno, EINVAL);

    rbh_backend_destroy(posix);
}
END_TEST

static const int RSST_ALL_FLAGS = AT_STATX_SYNC_TYPE;
static const int RSST_NOT_A_FLAG = -1;
static const int RSST_NOT_ONLY_A_FLAG = AT_STATX_SYNC_AS_STAT | -1;

static const void * const RSST_INVALIDS[] = {
    &RSST_ALL_FLAGS,
    &RSST_NOT_A_FLAG,
    &RSST_NOT_ONLY_A_FLAG,
    NULL,
};

static const void * const * const RPBO_INVALIDS[] = {
    [BO_INDEX(RBH_PBO_STATX_SYNC_TYPE)] = RSST_INVALIDS,
};

START_TEST(pbo_set_invalids)
{
    struct rbh_backend *posix;
    const void * const *data = RPBO_INVALIDS[BO_INDEX(_i)];
    size_t size = PBO_SIZES[BO_INDEX(_i)];

    posix = rbh_posix_backend_new(NULL, NULL, "", NULL);
    ck_assert_ptr_nonnull(posix);

    for (; *data != NULL; data++) {
        errno = 0;
        ck_assert_int_eq(rbh_backend_set_option(posix, _i, *data, size), -1);
        ck_assert_int_eq(errno, EINVAL);
    }

    rbh_backend_destroy(posix);
}
END_TEST

static const int RSST_FORCE_SYNC = AT_STATX_FORCE_SYNC;

static const void * const RSST_UNSUPPORTEDS[] = {
#ifndef HAVE_STATX
    &RSST_FORCE_SYNC,
#endif
    NULL,
};

static const void * const * const RPBO_UNSUPPORTEDS[] = {
    [BO_INDEX(RBH_PBO_STATX_SYNC_TYPE)] = RSST_UNSUPPORTEDS,
};

START_TEST(pbo_set_unsupporteds)
{
    struct rbh_backend *posix;
    const void * const *data = RPBO_UNSUPPORTEDS[BO_INDEX(_i)];
    size_t size = PBO_SIZES[BO_INDEX(_i)];

    posix = rbh_posix_backend_new(NULL, NULL, "", NULL);
    ck_assert_ptr_nonnull(posix);

    for (; *data != NULL; data++) {
        errno = 0;
        ck_assert_int_eq(rbh_backend_set_option(posix, _i, *data, size), -1);
        ck_assert_int_eq(errno, ENOTSUP);
    }

    rbh_backend_destroy(posix);
}
END_TEST

static const int RSST_SYNC_AS_STAT = AT_STATX_SYNC_AS_STAT;
static const int RSST_DONT_SYNC = AT_STATX_DONT_SYNC;

static const void * const RSST_VALIDS[] = {
    &RSST_SYNC_AS_STAT,
#ifdef HAVE_STATX
    &RSST_FORCE_SYNC,
#endif
    &RSST_DONT_SYNC,
    NULL,
};

static const void * const * const RBPO_VALIDS[] = {
    [BO_INDEX(RBH_PBO_STATX_SYNC_TYPE)] = RSST_VALIDS,
};

START_TEST(pbo_set_valids)
{
    struct rbh_backend *posix;
    const void * const *data = RBPO_VALIDS[BO_INDEX(_i)];
    size_t size = PBO_SIZES[BO_INDEX(_i)];
    void *value;

    posix = rbh_posix_backend_new(NULL, NULL, "", NULL);
    ck_assert_ptr_nonnull(posix);

    value = malloc(size);
    ck_assert_ptr_nonnull(value);

    for (; *data != NULL; data++) {
        ck_assert_int_eq(rbh_backend_set_option(posix, _i, *data, size), 0);
        ck_assert_int_eq(rbh_backend_get_option(posix, _i, value, &size), 0);
        ck_assert_uint_eq(size, PBO_SIZES[BO_INDEX(_i)]);
        ck_assert_mem_eq(value, *data, size);
    }

    free(value);
    rbh_backend_destroy(posix);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("posix backend");
    tests = tcase_create("filter");
    tcase_add_unchecked_fixture(tests, unchecked_setup_tmpdir,
                                unchecked_teardown_tmpdir);
    tcase_add_test(tests, pf_missing_root);
    tcase_add_test(tests, pf_empty_root);

    suite_add_tcase(suite, tests);

    tests = tcase_create("options");
    tcase_add_test(tests, pbo_get_unknown);
    tcase_add_test(tests, pbo_set_unknown);
    tcase_add_loop_test(tests, pbo_defaults, RBH_PBO_STATX_SYNC_TYPE, PBO_MAX);
    tcase_add_loop_test(tests, pbo_get_sizes, RBH_PBO_STATX_SYNC_TYPE, PBO_MAX);
    tcase_add_loop_test(tests, pbo_set_sizes, RBH_PBO_STATX_SYNC_TYPE, PBO_MAX);
    tcase_add_loop_test(tests, pbo_set_invalids, RBH_PBO_STATX_SYNC_TYPE,
                        PBO_MAX);
    tcase_add_loop_test(tests, pbo_set_unsupporteds, RBH_PBO_STATX_SYNC_TYPE,
                        PBO_MAX);
    tcase_add_loop_test(tests, pbo_set_valids, RBH_PBO_STATX_SYNC_TYPE,
                        PBO_MAX);

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
