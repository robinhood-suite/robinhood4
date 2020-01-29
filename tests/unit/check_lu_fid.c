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

#include "lu_fid.h"

#define ck_assert_lu_fid_eq(X, seq, oid, ver) do { \
    ck_assert_uint_eq((X)->f_seq, seq); \
    ck_assert_uint_eq((X)->f_oid, oid); \
    ck_assert_uint_eq((X)->f_ver, ver); \
} while (0)

/*----------------------------------------------------------------------------*
 |                         lu_fid_init_from_string()                          |
 *----------------------------------------------------------------------------*/

/* Technically, this function is not part of the public API of the library,
 * but since the symbol is not declared statically, we might has well test it.
 */

START_TEST(lfifs_basic)
{
    struct lu_fid fid;
    char *end;

    ck_assert_int_eq(lu_fid_init_from_string("0:1:2", &fid, &end), 0);
    ck_assert_int_eq(*end, '\0');
    ck_assert_lu_fid_eq(&fid, 0, 1, 2);
}
END_TEST

START_TEST(lfifs_empty)
{
    struct lu_fid fid;
    char *end;

    ck_assert_int_eq(lu_fid_init_from_string("::", &fid, &end), 0);
    ck_assert_int_eq(*end, '\0');
    ck_assert_lu_fid_eq(&fid, 0, 0, 0);
}
END_TEST

START_TEST(lfifs_bracket_enclosed)
{
    struct lu_fid fid;
    char *end;

    ck_assert_int_eq(lu_fid_init_from_string("[::]", &fid, &end), 0);
    ck_assert_int_eq(*end, '\0');
    ck_assert_lu_fid_eq(&fid, 0, 0, 0);
}
END_TEST

START_TEST(lfifs_missing_opening_bracket)
{
    struct lu_fid fid;
    char *end;

    ck_assert_int_eq(lu_fid_init_from_string("::]", &fid, &end), 0);
    ck_assert_int_eq(*end, ']');
    ck_assert_lu_fid_eq(&fid, 0, 0, 0);
}
END_TEST

START_TEST(lfifs_missing_closing_bracket)
{
    struct lu_fid fid;

    errno = 0;
    ck_assert_int_eq(lu_fid_init_from_string("[::", &fid, NULL), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(lfifs_garbage_in_sequence)
{
    struct lu_fid fid;

    errno = 0;
    ck_assert_int_eq(lu_fid_init_from_string("a::", &fid, NULL), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(lfifs_garbage_in_oid)
{
    struct lu_fid fid;

    errno = 0;
    ck_assert_int_eq(lu_fid_init_from_string(":a:", &fid, NULL), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(lfifs_garbage_in_version)
{
    struct lu_fid fid;
    char *end;

    ck_assert_int_eq(lu_fid_init_from_string("::a", &fid, &end), 0);
    ck_assert_int_eq(*end, 'a');
    ck_assert_lu_fid_eq(&fid, 0, 0, 0);
}
END_TEST

START_TEST(lfifs_hexa)
{
    struct lu_fid fid;
    char *end;

    ck_assert_int_eq(lu_fid_init_from_string("0x0:0x1:0x2", &fid, &end), 0);
    ck_assert_int_eq(*end, '\0');
    ck_assert_lu_fid_eq(&fid, 0, 1, 2);
}
END_TEST

START_TEST(lfifs_octal)
{
    struct lu_fid fid;
    char *end;

    ck_assert_int_eq(lu_fid_init_from_string("01:010:020", &fid, &end), 0);
    ck_assert_int_eq(*end, '\0');
    ck_assert_lu_fid_eq(&fid, 1, 8, 16);
}
END_TEST

START_TEST(lfifs_max_sequence)
{
    struct lu_fid fid;
    char *end;

    ck_assert_int_eq(
            lu_fid_init_from_string("0xffffffffffffffff::", &fid, &end), 0
            );
    ck_assert_int_eq(*end, '\0');
    ck_assert_lu_fid_eq(&fid, UINT64_MAX, 0, 0);
}
END_TEST

START_TEST(lfifs_sequence_overflow)
{
    struct lu_fid fid;

    errno = 0;
    ck_assert_int_eq(
            lu_fid_init_from_string("0x10000000000000000::", &fid, NULL), -1
            );
    ck_assert_int_eq(errno, ERANGE);
}
END_TEST

START_TEST(lfifs_max_oid)
{
    struct lu_fid fid;
    char *end;

    ck_assert_int_eq(lu_fid_init_from_string(":0xffffffff:", &fid, &end), 0);
    ck_assert_int_eq(*end, '\0');
    ck_assert_lu_fid_eq(&fid, 0, UINT32_MAX, 0);
}
END_TEST

START_TEST(lfifs_oid_overflow)
{
    struct lu_fid fid;

    errno = 0;
    ck_assert_int_eq(lu_fid_init_from_string(":0x100000000:", &fid, NULL), -1);
    ck_assert_int_eq(errno, ERANGE);
}
END_TEST

START_TEST(lfifs_max_version)
{
    struct lu_fid fid;
    char *end;

    ck_assert_int_eq(lu_fid_init_from_string("::0xffffffff", &fid, &end), 0);
    ck_assert_int_eq(*end, '\0');
    ck_assert_lu_fid_eq(&fid, 0, 0, UINT32_MAX);
}
END_TEST

START_TEST(lfifs_version_overflow)
{
    struct lu_fid fid;

    errno = 0;
    ck_assert_int_eq(lu_fid_init_from_string("::0x100000000", &fid, NULL), -1);
    ck_assert_int_eq(errno, ERANGE);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("lu_fid");
    tests = tcase_create("lu_fid_init_from_string()");
    tcase_add_test(tests, lfifs_empty);

    suite_add_tcase(suite, tests);
    tcase_add_test(tests, lfifs_basic);
    tcase_add_test(tests, lfifs_empty);
    tcase_add_test(tests, lfifs_bracket_enclosed);
    tcase_add_test(tests, lfifs_missing_opening_bracket);
    tcase_add_test(tests, lfifs_missing_closing_bracket);
    tcase_add_test(tests, lfifs_garbage_in_sequence);
    tcase_add_test(tests, lfifs_garbage_in_oid);
    tcase_add_test(tests, lfifs_garbage_in_version);
    tcase_add_test(tests, lfifs_hexa);
    tcase_add_test(tests, lfifs_octal);
    tcase_add_test(tests, lfifs_max_sequence);
    tcase_add_test(tests, lfifs_sequence_overflow);
    tcase_add_test(tests, lfifs_max_oid);
    tcase_add_test(tests, lfifs_oid_overflow);
    tcase_add_test(tests, lfifs_max_version);
    tcase_add_test(tests, lfifs_version_overflow);

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
