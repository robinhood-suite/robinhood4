/* This file is part of the RobinHood Library
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "check-compat.h"
#include "check_macros.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "robinhood/policyengine_internal.h"
#include "robinhood/backend.h"
#include "robinhood/utils.h"

/*
 * Test: rbh_collect_fsentries_test
 * Scenario: The filter is NULL (matches all). Verifies that an iterator is
 * returned and that next() yields NULL when the backend is empty.
 */
START_TEST(rbh_collect_fsentries_test)
{
    const char *uri = "rbh:mongo:test";
    struct rbh_backend *backend;
    struct rbh_mut_iterator *it;
    void *elem;

    backend = rbh_backend_from_uri(uri, true);
    ck_assert_ptr_nonnull(backend);

    errno = 0;
    it = rbh_collect_fsentries(backend, NULL);
    ck_assert_ptr_nonnull(it);

    errno = 0;
    elem = rbh_mut_iter_next(it);
    ck_assert_ptr_null(elem);
    ck_assert_int_eq(errno, ENODATA);

    rbh_mut_iter_destroy(it);
    rbh_backend_destroy(backend);
}
END_TEST

/*
 * Test: compare_values_success_test
 * Scenario: Validates that compare_values returns true for all supported types
 * and operators when values match the expected logic.
 */
START_TEST(compare_values_success_test)
{
    struct rbh_value a = {0};
    struct rbh_value b = {0};

    // int32 equality
    a.type = RBH_VT_INT32;
    a.int32 = 42;
    b.type = RBH_VT_INT32;
    b.int32 = 42;
    ck_assert(compare_values(RBH_FOP_EQUAL, &a, &b));

    // uint32 strictly lower
    a.type = RBH_VT_UINT32;
    a.uint32 = 5;
    b.type = RBH_VT_UINT32;
    b.uint32 = 10;
    ck_assert(compare_values(RBH_FOP_STRICTLY_LOWER, &a, &b));

    // int64 strictly lower
    a.type = RBH_VT_INT64;
    a.int64 = -100;
    b.type = RBH_VT_INT64;
    b.int64 = -50;
    ck_assert(compare_values(RBH_FOP_STRICTLY_LOWER, &a, &b));

    // uint64 strictly greater
    a.type = RBH_VT_UINT64;
    a.uint64 = 200;
    b.type = RBH_VT_UINT64;
    b.uint64 = 100;
    ck_assert(compare_values(RBH_FOP_STRICTLY_GREATER, &a, &b));

    // uint64 greater or equal
    a.type = RBH_VT_UINT64;
    a.uint64 = 100;
    b.type = RBH_VT_UINT64;
    b.uint64 = 100;
    ck_assert(compare_values(RBH_FOP_GREATER_OR_EQUAL, &a, &b));

    // string equality
    a.type = RBH_VT_STRING;
    a.string = "test";
    b.type = RBH_VT_STRING;
    b.string = "test";
    ck_assert(compare_values(RBH_FOP_EQUAL, &a, &b));
}
END_TEST

/*
 * Test: compare_values_failure_test
 * Scenario: Validates that compare_values returns false for incompatible types,
 * mismatched values, or invalid operators.
 */
START_TEST(compare_values_failure_test)
{
    struct rbh_value a = {0};
    struct rbh_value b = {0};

    // incompatible types (int32 vs uint32)
    a.type = RBH_VT_INT32;
    a.int32 = 10;
    b.type = RBH_VT_UINT32;
    b.uint32 = 10;
    ck_assert(!compare_values(RBH_FOP_EQUAL, &a, &b));

    // string not equal
    a.type = RBH_VT_STRING;
    a.string = "test";
    b.type = RBH_VT_STRING;
    b.string = "other";
    ck_assert(!compare_values(RBH_FOP_EQUAL, &a, &b));

    // string strictly lower (should fail)
    a.type = RBH_VT_STRING;
    a.string = "a";
    b.type = RBH_VT_STRING;
    b.string = "b";
    ck_assert(!compare_values(RBH_FOP_STRICTLY_LOWER, &a, &b));
    /* Also verify with equal strings: the operator is unsupported regardless
     * of the values. */
    a.string = "a";
    b.string = "a";
    ck_assert(!compare_values(RBH_FOP_STRICTLY_LOWER, &a, &b));

    // int32 strictly greater (should fail)
    a.type = RBH_VT_INT32;
    a.int32 = 5;
    b.type = RBH_VT_INT32;
    b.int32 = 10;
    ck_assert(!compare_values(RBH_FOP_STRICTLY_GREATER, &a, &b));

    // invalid operator
    a.type = RBH_VT_UINT64;
    a.uint64 = 100;
    b.type = RBH_VT_UINT64;
    b.uint64 = 200;
    ck_assert(!compare_values((enum rbh_filter_operator)999, &a, &b));
}
END_TEST

/*
 * Test: rbh_pe_execute_empty_iter_test
 * Scenario: Verifies that rbh_pe_execute returns 0 when called with an empty
 * iterator (mirror iterator is empty).
 */
START_TEST(rbh_pe_execute_empty_iter_test)
{
    const char *mirror_uri = "rbh:mongo:test";
    const char *fs_uri = "rbh:posix:/tmp";
    struct rbh_mut_iterator *mirror_iter;
    struct rbh_backend *mirror_backend;
    struct rbh_policy policy;
    int result;

    /* Create a simple policy */
    policy = (struct rbh_policy) {
        .name = "test_policy",
        .filter = NULL,
        .action = "test_action",
        .parameters = NULL,
        .rules = NULL,
        .rule_count = 0,
    };

    mirror_backend = rbh_backend_from_uri(mirror_uri, true);
    ck_assert_ptr_nonnull(mirror_backend);

    mirror_iter = rbh_collect_fsentries(mirror_backend, NULL);
    ck_assert_ptr_nonnull(mirror_iter);

    result = rbh_pe_execute(mirror_iter, mirror_backend, fs_uri, &policy);
    ck_assert_int_eq(result, 0);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite = suite_create("policyengine");
    TCase *tc;

    tc = tcase_create("collect_fsentries");
    tcase_add_test(tc, rbh_collect_fsentries_test);
    suite_add_tcase(suite, tc);

    tc = tcase_create("compare_values");
    tcase_add_test(tc, compare_values_success_test);
    tcase_add_test(tc, compare_values_failure_test);
    suite_add_tcase(suite, tc);

    tc = tcase_create("pe_execute");
    tcase_add_test(tc, rbh_pe_execute_empty_iter_test);
    suite_add_tcase(suite, tc);

    return suite;
}

int
main(void)
{
    int number_failed;
    Suite *suite = unit_suite();
    SRunner *runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
