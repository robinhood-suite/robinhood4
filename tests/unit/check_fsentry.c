/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>

#include <sys/stat.h>

#include "robinhood/fsentry.h"
#include "robinhood/statx.h"
#ifndef HAVE_STATX
# include "robinhood/statx-compat.h"
#endif

#include "check-compat.h"
#include "check_macros.h"
#include "utils.h"

/*----------------------------------------------------------------------------*
 |                             rbh_fsentry_new()                              |
 *----------------------------------------------------------------------------*/

START_TEST(rfn_empty)
{
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, NULL, NULL, NULL, NULL, NULL);
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

    fsentry = rbh_fsentry_new(&ID, NULL, NULL, NULL, NULL, NULL, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, RBH_FP_ID);
    ck_assert_id_eq(&fsentry->id, &ID);
    ck_assert_ptr_ne(fsentry->id.data, ID.data);
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

    fsentry = rbh_fsentry_new(NULL, &PARENT_ID, NULL, NULL, NULL, NULL, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, RBH_FP_PARENT_ID);
    ck_assert_id_eq(&fsentry->parent_id, &PARENT_ID);
    ck_assert_ptr_ne(fsentry->parent_id.data, PARENT_ID.data);
    free(fsentry);
}
END_TEST

START_TEST(rfn_name)
{
    static const char NAME[] = "abcdefg";
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, NAME, NULL, NULL, NULL, NULL);
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
        .stx_mask = RBH_STATX_UID,
        .stx_uid = 1,
    };
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, NULL, &STATX, NULL, NULL, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, RBH_FP_STATX);
    ck_assert_ptr_ne(fsentry->statx, &STATX);
    ck_assert_mem_eq(fsentry->statx, &STATX, sizeof(STATX));
    free(fsentry);
}
END_TEST

START_TEST(rfn_statx_misaligned)
{
    static const struct statx STATX = {};
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, "abcdef", &STATX, NULL, NULL, NULL);
    ck_assert_ptr_nonnull(fsentry);
    /* Access a member of the statx struct to trigger the misaligned access */
    ck_assert_int_eq(fsentry->statx->stx_mask, 0);
    free(fsentry);
}
END_TEST

START_TEST(rfn_ns_xattrs)
{
    static const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    static const struct rbh_value_pair PAIR = {
        .key = "abcdefg",
        .value = &VALUE,
    };
    static const struct rbh_value_map XATTRS = {
        .pairs = &PAIR,
        .count = 1,
    };
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, NULL, NULL, &XATTRS, NULL, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, RBH_FP_NAMESPACE_XATTRS);
    ck_assert_value_map_eq(&fsentry->xattrs.ns, &XATTRS);
    free(fsentry);
}
END_TEST

START_TEST(rfn_ns_xattrs_misaligned)
{
    static const struct rbh_value VALUE = {
        .type = RBH_VT_UINT32,
        .uint32 = 0,
    };
    static const struct rbh_value_pair PAIR = {
        .key = "abcdefg",
        .value = &VALUE,
    };
    static const struct rbh_value_map XATTRS = {
        .pairs = &PAIR,
        .count = 1,
    };
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, "abcdef", NULL, &XATTRS, NULL, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_value_map_eq(&fsentry->xattrs.ns, &XATTRS);
    free(fsentry);
}
END_TEST

START_TEST(rfn_inode_xattrs)
{
    static const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    static const struct rbh_value_pair PAIR = {
            .key = "abcdefg",
            .value = &VALUE,
    };
    static const struct rbh_value_map XATTRS = {
        .pairs = &PAIR,
        .count = 1,
    };
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, NULL, NULL, NULL, &XATTRS, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_int_eq(fsentry->mask, RBH_FP_INODE_XATTRS);
    ck_assert_value_map_eq(&fsentry->xattrs.inode, &XATTRS);
    free(fsentry);
}
END_TEST

START_TEST(rfn_inode_xattrs_misaligned)
{
    static const struct rbh_value VALUE = {
        .type = RBH_VT_UINT32,
        .uint32 = 0,
    };
    static const struct rbh_value_pair PAIR = {
            .key = "abcdefg",
            .value = &VALUE,
    };
    static const struct rbh_value_map XATTRS = {
        .pairs = &PAIR,
        .count = 1,
    };
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, "abcdef", NULL, NULL, &XATTRS, NULL);
    ck_assert_ptr_nonnull(fsentry);
    ck_assert_value_map_eq(&fsentry->xattrs.inode, &XATTRS);
    free(fsentry);
}
END_TEST

START_TEST(rfn_symlink)
{
    static const char SYMLINK[] = "abcdefg";
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(NULL, NULL, NULL, NULL, NULL, NULL, SYMLINK);
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
        .stx_mask = RBH_STATX_TYPE,
        .stx_mode = S_IFREG,
    };
    static const char SYMLINK[] = "abcdefg";

    errno = 0;
    ck_assert_ptr_null(
            rbh_fsentry_new(NULL, NULL, NULL, &STATX, NULL, NULL, SYMLINK)
            );
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
        .stx_mask = RBH_STATX_UID,
        .stx_uid = 0,
    };
    static const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    static const struct rbh_value_pair PAIR = {
            .key = "abcdefg",
            .value = &VALUE,
    };
    static const struct rbh_value_map XATTRS = {
        .pairs = &PAIR,
        .count = 1,
    };
    static const char SYMLINK[] = "hijklmn";
    struct rbh_fsentry *fsentry;

    fsentry = rbh_fsentry_new(&ID, &PARENT_ID, NAME, &STATX, &XATTRS, &XATTRS,
                              SYMLINK);
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
    /* xattrs.ns */
    ck_assert_value_map_eq(&fsentry->xattrs.ns, &XATTRS);
    /* xattrs.inode */
    ck_assert_value_map_eq(&fsentry->xattrs.inode, &XATTRS);
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
    tcase_add_test(tests, rfn_statx_misaligned);
    tcase_add_test(tests, rfn_ns_xattrs);
    tcase_add_test(tests, rfn_ns_xattrs_misaligned);
    tcase_add_test(tests, rfn_inode_xattrs);
    tcase_add_test(tests, rfn_inode_xattrs_misaligned);
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
