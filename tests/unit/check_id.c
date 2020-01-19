/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#include "check-compat.h"
#include "check_macros.h"
#include "robinhood/id.h"

/*----------------------------------------------------------------------------*
 |                               rbh_id_copy()                                |
 *----------------------------------------------------------------------------*/

START_TEST(ric_basic)
{
    const char DATA[] = "abcdefg";
    const struct rbh_id ID = {
        .data = DATA,
        .size = sizeof(DATA),
    };
    size_t size = sizeof(DATA);
    char buffer[sizeof(DATA)];
    char *data = buffer;
    struct rbh_id id;

    ck_assert_int_eq(rbh_id_copy(&id, &ID, &data, &size), 0);
    /* `id' must not point at DATA */
    ck_assert_ptr_ne(id.data, DATA);
    /* `id' should point at buffer */
    ck_assert_ptr_eq(id.data, buffer);
    ck_assert_id_eq(&id, &ID);

    /* Check `data' and `size' were correctly updated */
    ck_assert_ptr_eq(data, buffer + sizeof(DATA));
    ck_assert_uint_eq(size, 0);
}
END_TEST

START_TEST(ric_enobufs)
{
    const char DATA[] = "abcdefg";
    const struct rbh_id ID = {
        .data = DATA,
        .size = sizeof(DATA),
    };
    size_t size = sizeof(DATA) - 1;
    char *data;

    errno = 0;
    ck_assert_int_eq(rbh_id_copy(NULL, &ID, &data, &size), -1);
    ck_assert_int_eq(errno, ENOBUFS);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                                rbh_id_new()                                |
 *----------------------------------------------------------------------------*/

START_TEST(rin_basic)
{
    const char DATA[] = "abcdefg";
    const struct rbh_id ID = {
        .data = DATA,
        .size = sizeof(DATA),
    };
    struct rbh_id *id;

    id = rbh_id_new(DATA, sizeof(DATA));
    ck_assert_ptr_nonnull(id);
    ck_assert_ptr_ne(id->data, DATA);
    ck_assert_id_eq(id, &ID);

    free(id);
}
END_TEST

START_TEST(rin_empty)
{
    const struct rbh_id ID = {
        .data = NULL,
        .size = 0,
    };
    struct rbh_id *id;

    id = rbh_id_new(NULL, 0);
    ck_assert_ptr_nonnull(id);
    ck_assert_id_eq(id, &ID);

    free(id);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                         rbh_id_from_file_handle()                          |
 *----------------------------------------------------------------------------*/

START_TEST(riffh_sizeof_handle_type)
{
    struct file_handle *fh;

    /* Currently the handle_type of file handles is declared as an `int'.
     * If that ever changes, it will break RobinHood's ability to convert
     * an ID back into a file handle.
     *
     * There is no reason for this to happen though.
     */
    ck_assert_uint_eq(sizeof(fh->handle_type), sizeof(int));
}
END_TEST

/* The following test only ensures the binary layout of an ID built from a
 * file handle is consistent over time.
 */

START_TEST(riffh_basic)
{
    struct file_handle *fh;
    const char F_HANDLE[] = "abcdefg";
    char fh_buffer[sizeof(*fh) + sizeof(F_HANDLE)];
    char data[sizeof(int) + sizeof(F_HANDLE)];
    const struct rbh_id ID = {
        .data = data,
        .size = sizeof(data),
    };
    struct rbh_id *id;
    char *tmp;

    fh = (struct file_handle *)fh_buffer;
    fh->handle_bytes = sizeof(F_HANDLE);
    fh->handle_type = 0x01234567;
    memcpy(fh->f_handle, F_HANDLE, sizeof(F_HANDLE));

    tmp = mempcpy(data, &fh->handle_type, sizeof(int));
    memcpy(tmp, F_HANDLE, sizeof(F_HANDLE));

    id = rbh_id_from_file_handle(fh);
    ck_assert_ptr_nonnull(id);
    ck_assert_id_eq(id, &ID);

    free(id);
}
END_TEST

START_TEST(riffh_empty)
{
    const struct file_handle FH = {
        .handle_bytes = 0,
        .handle_type = 0x01234567,
        .f_handle = "",
    };
    const struct rbh_id ID = {
        .data = (char *)&FH.handle_type,
        .size = sizeof(FH.handle_type),
    };
    struct rbh_id *id;

    id = rbh_id_from_file_handle(&FH);
    ck_assert_ptr_nonnull(id);
    ck_assert_id_eq(id, &ID);

    free(id);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("ID");
    tests = tcase_create("rbh_id_copy()");
    tcase_add_test(tests, ric_basic);
    tcase_add_test(tests, ric_enobufs);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_id_new()");
    tcase_add_test(tests, rin_basic);
    tcase_add_test(tests, rin_empty);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_id_from_file_handle()");
    tcase_add_test(tests, riffh_sizeof_handle_type);
    tcase_add_test(tests, riffh_basic);
    tcase_add_test(tests, riffh_empty);

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
