/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>

#include <sys/stat.h>

#include "check-compat.h"
#include "robinhood/fsentry.h"
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

/*----------------------------------------------------------------------------*
 |                             rbh_fsentry_new()                              |
 *----------------------------------------------------------------------------*/

START_TEST(rfn_empty)
{
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, NULL, NULL, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, 0);
    free(fsentry);
}
END_TEST

START_TEST(rfn_id)
{
    static const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(&ID, NULL, NULL, NULL, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, RBH_FP_ID);
    ck_assert_int_eq(fsentry->id.size, ID.size);
    ck_assert_ptr_ne(fsentry->id.data, ID.data);
    ck_assert_mem_eq(fsentry->id.data, ID.data, ID.size);
    free(fsentry);
}
END_TEST

START_TEST(rfn_parent_id)
{
    static const struct rbh_id PARENT_ID = {
        .data = "abcdefg",
        .size = 8,
    };
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, &PARENT_ID, NULL, NULL, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, RBH_FP_PARENT_ID);
    ck_assert_int_eq(fsentry->parent_id.size, PARENT_ID.size);
    ck_assert_ptr_ne(fsentry->parent_id.data, PARENT_ID.data);
    ck_assert_mem_eq(fsentry->parent_id.data, PARENT_ID.data, PARENT_ID.size);
    free(fsentry);
}
END_TEST

START_TEST(rfn_name)
{
    static const char NAME[] = "abcdefg";
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, NAME, NULL, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, RBH_FP_NAME);
    ck_assert_ptr_ne(fsentry->name, NAME);
    ck_assert_str_eq(fsentry->name, NAME);
    free(fsentry);
}
END_TEST

START_TEST(rfn_statx)
{
    static const struct statx STATX = {
        .stx_mask = STATX_UID,
        .stx_uid = 0,
    };
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, NULL, &STATX, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, RBH_FP_STATX);
    ck_assert_ptr_ne(fsentry->statx, &STATX);
    ck_assert_mem_eq(fsentry->statx, &STATX, sizeof(STATX));
    free(fsentry);
}
END_TEST

START_TEST(rfn_symlink)
{
    static const char SYMLINK[] = "abcdefg";
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, NULL, NULL, SYMLINK);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, RBH_FP_SYMLINK);
    ck_assert_ptr_ne(fsentry->symlink, SYMLINK);
    ck_assert_str_eq(fsentry->symlink, SYMLINK);
    free(fsentry);
}
END_TEST

START_TEST(rfn_symlink_not_a_symlink)
{
    static const struct statx STATX = {
        .stx_mask = STATX_TYPE,
        .stx_mode = S_IFREG,
    };
    static const char SYMLINK[] = "abcdefg";

    errno = 0;
    ck_assert_ptr_null(rbh_fsentry_new(NULL, NULL, NULL, &STATX, SYMLINK));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfn_all)
{
    static const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };
    static const struct rbh_id PARENT_ID = {
        .data = "hijklmn",
        .size = 8,
    };
    static const char NAME[] = "opqrstu";
    static const struct statx STATX = {
        .stx_mask = STATX_UID,
        .stx_uid = 0,
    };
    static const char SYMLINK[] = "hijklmn";
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(&ID, &PARENT_ID, NAME, &STATX, SYMLINK);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, RBH_FP_ALL);
    /* id */
    ck_assert_int_eq(fsentry->id.size, ID.size);
    ck_assert_ptr_ne(fsentry->id.data, ID.data);
    ck_assert_mem_eq(fsentry->id.data, ID.data, ID.size);
    /* parent_id */
    ck_assert_int_eq(fsentry->parent_id.size, PARENT_ID.size);
    ck_assert_ptr_ne(fsentry->parent_id.data, PARENT_ID.data);
    ck_assert_mem_eq(fsentry->parent_id.data, PARENT_ID.data, PARENT_ID.size);
    /* name */
    ck_assert_ptr_ne(fsentry->name, NAME);
    ck_assert_str_eq(fsentry->name, NAME);
    /* statx */
    ck_assert_ptr_ne(fsentry->statx, &STATX);
    ck_assert_mem_eq(fsentry->statx, &STATX, sizeof(STATX));
    /* symlink */
    ck_assert_ptr_ne(fsentry->symlink, SYMLINK);
    ck_assert_str_eq(fsentry->symlink, SYMLINK);

    free(fsentry);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("fsentry");
    tests = tcase_create("rbh_fsentry_new()");
    tcase_add_test(tests, rfn_empty);
    tcase_add_test(tests, rfn_id);
    tcase_add_test(tests, rfn_parent_id);
    tcase_add_test(tests, rfn_name);
    tcase_add_test(tests, rfn_statx);
    tcase_add_test(tests, rfn_symlink);
    tcase_add_test(tests, rfn_symlink_not_a_symlink);
    tcase_add_test(tests, rfn_all);

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
