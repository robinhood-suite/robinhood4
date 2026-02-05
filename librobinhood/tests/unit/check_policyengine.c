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

#include "robinhood/backend.h"
#include "robinhood/utils.h"
#include "robinhood/policyengine.h"

/* The filter is NULL (matches all). We only verify that an iterator is
 * returned and that next() yields NULL when the backend is empty.
 */
START_TEST(rbh_collect_fsentries_test)
{
    const char *uri = "rbh:mongo:test";
    struct rbh_backend *backend = rbh_backend_from_uri(uri, true);
    ck_assert_ptr_nonnull(backend);

    errno = 0;
    struct rbh_mut_iterator *it = rbh_collect_fsentries(backend, NULL);
    ck_assert_ptr_nonnull(it);

    errno = 0;
    void *elem = rbh_mut_iter_next(it);
    ck_assert_ptr_null(elem);
    ck_assert_int_eq(errno, ENODATA);

    rbh_mut_iter_destroy(it);
    rbh_backend_destroy(backend);
}
END_TEST

/* Test rbh_pe_execute with an empty iterator.
 * Verify that the function returns 0 when the
 * mirror iterator is empty.
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
    TCase *tc_collect = tcase_create("collect_fsentries");
    tcase_add_test(tc_collect, rbh_collect_fsentries_test);
    suite_add_tcase(suite, tc_collect);

    TCase *tc_execute = tcase_create("pe_execute");
    tcase_add_test(tc_execute, rbh_pe_execute_empty_iter_test);
    suite_add_tcase(suite, tc_execute);

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
