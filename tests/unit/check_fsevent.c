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
#include "robinhood/fsevent.h"
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

/*----------------------------------------------------------------------------*
 |                          rbh_fsevent_upsert_new()                          |
 *----------------------------------------------------------------------------*/

START_TEST(rfupn_basic)
{
    static const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_upsert_new(&ID, NULL, NULL);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_int_eq(fsevent->type, RBH_FET_UPSERT);
    ck_assert_uint_eq(fsevent->id.size, ID.size);
    ck_assert_ptr_ne(fsevent->id.data, ID.data);
    ck_assert_mem_eq(fsevent->id.data, ID.data, ID.size);

    ck_assert_ptr_null(fsevent->upsert.statx);
    ck_assert_ptr_null(fsevent->upsert.symlink);

    free(fsevent);
}
END_TEST

START_TEST(rfupn_statx)
{
    static const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };
    static const struct statx STATX = {
        .stx_mask = STATX_UID,
        .stx_uid = 0,
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_upsert_new(&ID, &STATX, NULL);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_int_eq(fsevent->type, RBH_FET_UPSERT);
    ck_assert_uint_eq(fsevent->id.size, ID.size);
    ck_assert_ptr_ne(fsevent->id.data, ID.data);
    ck_assert_mem_eq(fsevent->id.data, ID.data, ID.size);

    ck_assert_ptr_nonnull(fsevent->upsert.statx);
    ck_assert_ptr_ne(fsevent->upsert.statx, &STATX);
    ck_assert_mem_eq(fsevent->upsert.statx, &STATX, sizeof(STATX));

    ck_assert_ptr_null(fsevent->upsert.symlink);

    free(fsevent);
}
END_TEST

START_TEST(rfupn_symlink)
{
    static const struct rbh_id ID = {
        .data ="abcdefg",
        .size = 8,
    };
    static const char SYMLINK[] = "hijklmn";
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_upsert_new(&ID, NULL, SYMLINK);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_int_eq(fsevent->type, RBH_FET_UPSERT);
    ck_assert_uint_eq(fsevent->id.size, ID.size);
    ck_assert_ptr_ne(fsevent->id.data, ID.data);
    ck_assert_mem_eq(fsevent->id.data, ID.data, ID.size);

    ck_assert_ptr_null(fsevent->upsert.statx);

    ck_assert_ptr_nonnull(fsevent->upsert.symlink);
    ck_assert_ptr_ne(fsevent->upsert.symlink, SYMLINK);
    ck_assert_str_eq(fsevent->upsert.symlink, SYMLINK);

    free(fsevent);
}
END_TEST

START_TEST(rfupn_symlink_not_a_symlink)
{
    static const struct rbh_id ID = {
        .data ="abcdefg",
        .size = 8,
    };
    static const struct statx STATX = {
        .stx_mask = STATX_TYPE,
        .stx_mode = S_IFREG,
    };
    static const char SYMLINK[] = "hijklmn";
    struct rbh_fsevent *fsevent;

    errno = 0;
    fsevent = rbh_fsevent_upsert_new(&ID, &STATX, SYMLINK);
    ck_assert_ptr_null(fsevent);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfupn_all)
{
    static const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };
    static const struct statx STATX = {
        .stx_mask = STATX_UID,
        .stx_uid = 0,
    };
    static const char SYMLINK[] = "hijklmn";
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_upsert_new(&ID, &STATX, SYMLINK);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_int_eq(fsevent->type, RBH_FET_UPSERT);
    ck_assert_uint_eq(fsevent->id.size, ID.size);
    ck_assert_ptr_ne(fsevent->id.data, ID.data);
    ck_assert_mem_eq(fsevent->id.data, ID.data, ID.size);

    ck_assert_ptr_nonnull(fsevent->upsert.statx);
    ck_assert_ptr_ne(fsevent->upsert.statx, &STATX);
    ck_assert_mem_eq(fsevent->upsert.statx, &STATX, sizeof(STATX));

    ck_assert_ptr_ne(fsevent->upsert.symlink, SYMLINK);
    ck_assert_str_eq(fsevent->upsert.symlink, SYMLINK);

    free(fsevent);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_fsevent_link_new()                           |
 *----------------------------------------------------------------------------*/

START_TEST(rfln_basic)
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
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_link_new(&ID, &PARENT_ID, NAME);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_int_eq(fsevent->type, RBH_FET_LINK);
    ck_assert_uint_eq(fsevent->id.size, ID.size);
    ck_assert_ptr_ne(fsevent->id.data, ID.data);
    ck_assert_mem_eq(fsevent->id.data, ID.data, ID.size);

    ck_assert_ptr_nonnull(fsevent->link.parent_id);
    ck_assert_uint_eq(fsevent->link.parent_id->size, PARENT_ID.size);
    ck_assert_ptr_ne(fsevent->link.parent_id->data, PARENT_ID.data);
    ck_assert_mem_eq(fsevent->link.parent_id->data, PARENT_ID.data,
                     PARENT_ID.size);

    ck_assert_ptr_nonnull(fsevent->link.name);
    ck_assert_ptr_ne(fsevent->link.name, NAME);
    ck_assert_str_eq(fsevent->link.name, NAME);

    free(fsevent);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                          rbh_fsevent_unlink_new()                          |
 *----------------------------------------------------------------------------*/

/* rbh_fsevent_unlink_new() uses the same underlying implementation as
 * rbh_fsevent_link_new(), there is no need to test it extensively
 */
START_TEST(rfuln_basic)
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
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_unlink_new(&ID, &PARENT_ID, NAME);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_int_eq(fsevent->type, RBH_FET_UNLINK);

    free(fsevent);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                          rbh_fsevent_delete_new()                          |
 *----------------------------------------------------------------------------*/

START_TEST(rfdn_basic)
{
    static const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_delete_new(&ID);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_int_eq(fsevent->type, RBH_FET_DELETE);
    ck_assert_uint_eq(fsevent->id.size, ID.size);
    ck_assert_ptr_ne(fsevent->id.data, ID.data);
    ck_assert_mem_eq(fsevent->id.data, ID.data, ID.size);

    free(fsevent);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("fsevent");
    tests = tcase_create("rbh_fsevent_upsert_new()");
    tcase_add_test(tests, rfupn_basic);
    tcase_add_test(tests, rfupn_statx);
    tcase_add_test(tests, rfupn_symlink);
    tcase_add_test(tests, rfupn_symlink_not_a_symlink);
    tcase_add_test(tests, rfupn_all);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_fsevent_link_new()");
    tcase_add_test(tests, rfln_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_fsevent_unlink_new()");
    tcase_add_test(tests, rfuln_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_fsevent_delete_new()");
    tcase_add_test(tests, rfdn_basic);

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
