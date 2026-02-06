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
#include "robinhood/statx.h"

/*
 * Test: rbh_collect_fsentries_test
 * Scenario: The filter is NULL (matches all). Verifies that an iterator is
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

/*
 * Test: rbh_filter_matches_fsentry_null_filter_test
 * Scenario: Validates that a NULL filter matches any fsentry (returns true).
 */
START_TEST(rbh_filter_matches_fsentry_null_filter_test)
{
    struct rbh_fsentry fsentry = {0};
    fsentry.mask = RBH_FP_STATX;

    // NULL filter matches all
    ck_assert(rbh_filter_matches_fsentry(NULL, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_equality_match_test
 * Scenario: Validates that an equality filter matches when the field value
 * equals the filter value.
 */
START_TEST(rbh_filter_matches_fsentry_equality_match_test)
{
    struct rbh_statx statx = {0};
    statx.stx_mask = RBH_STATX_SIZE;
    statx.stx_size = 1024;

    struct rbh_fsentry fsentry = {0};
    fsentry.mask = RBH_FP_STATX;
    fsentry.statx = &statx;

    struct rbh_value filter_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1024
    };

    struct rbh_filter_field field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };

    struct rbh_filter filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = field,
            .value = filter_value
        }
    };

    // size == 1024
    ck_assert(rbh_filter_matches_fsentry(&filter, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_equality_no_match_test
 * Scenario: Validates that an equality filter returns false when values don't
 * match.
 */
START_TEST(rbh_filter_matches_fsentry_equality_no_match_test)
{
    struct rbh_statx statx = {0};
    statx.stx_mask = RBH_STATX_SIZE;
    statx.stx_size = 2048;

    struct rbh_fsentry fsentry = {0};
    fsentry.mask = RBH_FP_STATX;
    fsentry.statx = &statx;

    struct rbh_value filter_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1024
    };

    struct rbh_filter_field field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };

    struct rbh_filter filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = field,
            .value = filter_value
        }
    };

    // size != 1024
    ck_assert(!rbh_filter_matches_fsentry(&filter, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_greater_match_test
 * Scenario: Validates that STRICTLY_GREATER filter matches when field value is
 * greater.
 */
START_TEST(rbh_filter_matches_fsentry_greater_match_test)
{
    struct rbh_statx statx = {0};
    statx.stx_mask = RBH_STATX_SIZE;
    statx.stx_size = 2048;

    struct rbh_fsentry fsentry = {0};
    fsentry.mask = RBH_FP_STATX;
    fsentry.statx = &statx;

    struct rbh_value filter_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1024
    };

    struct rbh_filter_field field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };

    struct rbh_filter filter = {
        .op = RBH_FOP_STRICTLY_GREATER,
        .compare = {
            .field = field,
            .value = filter_value
        }
    };

    // size > 1024
    ck_assert(rbh_filter_matches_fsentry(&filter, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_greater_no_match_test
 * Scenario: Validates that STRICTLY_GREATER filter returns false when field
 * value is not greater.
 */
START_TEST(rbh_filter_matches_fsentry_greater_no_match_test)
{
    struct rbh_statx statx = {0};
    statx.stx_mask = RBH_STATX_SIZE;
    statx.stx_size = 512;

    struct rbh_fsentry fsentry = {0};
    fsentry.mask = RBH_FP_STATX;
    fsentry.statx = &statx;

    struct rbh_value filter_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1024
    };

    struct rbh_filter_field field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };

    struct rbh_filter filter = {
        .op = RBH_FOP_STRICTLY_GREATER,
        .compare = {
            .field = field,
            .value = filter_value
        }
    };

    // size <= 1024
    ck_assert(!rbh_filter_matches_fsentry(&filter, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_missing_field_test
 * Scenario: Validates that filter returns false when the field is not present
 * in fsentry.
 */
START_TEST(rbh_filter_matches_fsentry_missing_field_test)
{
    struct rbh_fsentry fsentry = {0};
    fsentry.mask = 0;  // no fields set

    struct rbh_value filter_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1024
    };

    struct rbh_filter_field field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };

    struct rbh_filter filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = field,
            .value = filter_value
        }
    };

    // field absent
    ck_assert(!rbh_filter_matches_fsentry(&filter, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_and_all_true_test
 * Scenario: Validates that AND filter returns true when all conditions are
 * satisfied.
 */
START_TEST(rbh_filter_matches_fsentry_and_all_true_test)
{
    struct rbh_statx statx = {0};
    statx.stx_mask = RBH_STATX_SIZE | RBH_STATX_UID;
    statx.stx_size = 2048;
    statx.stx_uid = 1000;

    struct rbh_fsentry fsentry = {0};
    fsentry.mask = RBH_FP_STATX;
    fsentry.statx = &statx;

    // size > 1024
    struct rbh_filter_field size_field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };
    struct rbh_value size_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1024
    };
    struct rbh_filter size_filter = {
        .op = RBH_FOP_STRICTLY_GREATER,
        .compare = {
            .field = size_field,
            .value = size_value
        }
    };

    // uid == 1000
    struct rbh_filter_field uid_field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_UID
    };
    struct rbh_value uid_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1000
    };
    struct rbh_filter uid_filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = uid_field,
            .value = uid_value
        }
    };

    const struct rbh_filter *sub_filters[] = {&size_filter, &uid_filter};
    struct rbh_filter and_filter = {
        .op = RBH_FOP_AND,
        .logical = {
            .filters = sub_filters,
            .count = 2
        }
    };

    // both conditions true
    ck_assert(rbh_filter_matches_fsentry(&and_filter, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_and_one_false_test
 * Scenario: Validates that AND filter returns false when at least one
 * condition is not satisfied.
 */
START_TEST(rbh_filter_matches_fsentry_and_one_false_test)
{
    struct rbh_statx statx = {0};
    statx.stx_mask = RBH_STATX_SIZE | RBH_STATX_UID;
    statx.stx_size = 512;  // fails size > 1024
    statx.stx_uid = 1000;

    struct rbh_fsentry fsentry = {0};
    fsentry.mask = RBH_FP_STATX;
    fsentry.statx = &statx;

    // size > 1024 (will fail)
    struct rbh_filter_field size_field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };
    struct rbh_value size_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1024
    };
    struct rbh_filter size_filter = {
        .op = RBH_FOP_STRICTLY_GREATER,
        .compare = {
            .field = size_field,
            .value = size_value
        }
    };

    // uid == 1000 (will pass)
    struct rbh_filter_field uid_field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_UID
    };
    struct rbh_value uid_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1000
    };
    struct rbh_filter uid_filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = uid_field,
            .value = uid_value
        }
    };

    const struct rbh_filter *sub_filters[] = {&size_filter, &uid_filter};
    struct rbh_filter and_filter = {
        .op = RBH_FOP_AND,
        .logical = {
            .filters = sub_filters,
            .count = 2
        }
    };

    // one condition false
    ck_assert(!rbh_filter_matches_fsentry(&and_filter, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_or_one_true_test
 * Scenario: Validates that OR filter returns true when at least one condition
 * is satisfied.
 */
START_TEST(rbh_filter_matches_fsentry_or_one_true_test)
{
    struct rbh_statx statx = {0};
    statx.stx_mask = RBH_STATX_SIZE | RBH_STATX_UID;
    statx.stx_size = 512;  // fails size > 1024
    statx.stx_uid = 1000;  // passes uid == 1000

    struct rbh_fsentry fsentry = {0};
    fsentry.mask = RBH_FP_STATX;
    fsentry.statx = &statx;

    // size > 1024 (will fail)
    struct rbh_filter_field size_field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };
    struct rbh_value size_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1024
    };
    struct rbh_filter size_filter = {
        .op = RBH_FOP_STRICTLY_GREATER,
        .compare = {
            .field = size_field,
            .value = size_value
        }
    };

    // uid == 1000 (will pass)
    struct rbh_filter_field uid_field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_UID
    };
    struct rbh_value uid_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1000
    };
    struct rbh_filter uid_filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = uid_field,
            .value = uid_value
        }
    };

    const struct rbh_filter *sub_filters[] = {&size_filter, &uid_filter};
    struct rbh_filter or_filter = {
        .op = RBH_FOP_OR,
        .logical = {
            .filters = sub_filters,
            .count = 2
        }
    };

    // at least one condition true
    ck_assert(rbh_filter_matches_fsentry(&or_filter, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_or_all_false_test
 * Scenario: Validates that OR filter returns false when all conditions are not
 * satisfied.
 */
START_TEST(rbh_filter_matches_fsentry_or_all_false_test)
{
    struct rbh_statx statx = {0};
    statx.stx_mask = RBH_STATX_SIZE | RBH_STATX_UID;
    statx.stx_size = 512;  // fails size > 1024
    statx.stx_uid = 500;   // fails uid == 1000

    struct rbh_fsentry fsentry = {0};
    fsentry.mask = RBH_FP_STATX;
    fsentry.statx = &statx;

    // size > 1024 (will fail)
    struct rbh_filter_field size_field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };
    struct rbh_value size_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1024
    };
    struct rbh_filter size_filter = {
        .op = RBH_FOP_STRICTLY_GREATER,
        .compare = {
            .field = size_field,
            .value = size_value
        }
    };

    // uid == 1000 (will fail)
    struct rbh_filter_field uid_field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_UID
    };
    struct rbh_value uid_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1000
    };
    struct rbh_filter uid_filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = uid_field,
            .value = uid_value
        }
    };

    const struct rbh_filter *sub_filters[] = {&size_filter, &uid_filter};
    struct rbh_filter or_filter = {
        .op = RBH_FOP_OR,
        .logical = {
            .filters = sub_filters,
            .count = 2
        }
    };

    // all conditions false
    ck_assert(!rbh_filter_matches_fsentry(&or_filter, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_not_test
 * Scenario: Validates that NOT filter inverts the result of the inner filter.
 */
START_TEST(rbh_filter_matches_fsentry_not_test)
{
    struct rbh_statx statx = {0};
    statx.stx_mask = RBH_STATX_SIZE;
    statx.stx_size = 512;

    struct rbh_fsentry fsentry = {0};
    fsentry.mask = RBH_FP_STATX;
    fsentry.statx = &statx;

    // size > 1024 (will fail for size=512)
    struct rbh_filter_field size_field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };
    struct rbh_value size_value = {
        .type = RBH_VT_UINT64,
        .uint64 = 1024
    };
    struct rbh_filter size_filter = {
        .op = RBH_FOP_STRICTLY_GREATER,
        .compare = {
            .field = size_field,
            .value = size_value
        }
    };

    const struct rbh_filter *sub_filter = &size_filter;
    struct rbh_filter not_filter = {
        .op = RBH_FOP_NOT,
        .logical = {
            .filters = &sub_filter,
            .count = 1
        }
    };

    // NOT(size > 1024) = true (because size <= 1024)
    ck_assert(rbh_filter_matches_fsentry(&not_filter, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_exists_present_test
 * Scenario: Validates that EXISTS filter returns true when the field is
 * present.
 */
START_TEST(rbh_filter_matches_fsentry_exists_present_test)
{
    struct rbh_statx statx = {0};
    statx.stx_mask = RBH_STATX_SIZE;
    statx.stx_size = 1024;

    struct rbh_fsentry fsentry = {0};
    fsentry.mask = RBH_FP_STATX;
    fsentry.statx = &statx;

    struct rbh_filter_field size_field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };

    struct rbh_filter exists_filter = {
        .op = RBH_FOP_EXISTS,
        .compare = {
            .field = size_field
        }
    };

    // field present
    ck_assert(rbh_filter_matches_fsentry(&exists_filter, &fsentry));
}
END_TEST

/*
 * Test: rbh_filter_matches_fsentry_exists_absent_test
 * Scenario: Validates that EXISTS filter returns false when the field is
 * absent.
 */
START_TEST(rbh_filter_matches_fsentry_exists_absent_test)
{
    struct rbh_fsentry fsentry = {0};
    fsentry.mask = 0;  // no fields set

    struct rbh_filter_field size_field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_SIZE
    };

    struct rbh_filter exists_filter = {
        .op = RBH_FOP_EXISTS,
        .compare = {
            .field = size_field
        }
    };

    // field absent
    ck_assert(!rbh_filter_matches_fsentry(&exists_filter, &fsentry));
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite = suite_create("policyengine");
    TCase *tc_collect = tcase_create("collect_fsentries");
    tcase_add_test(tc_collect, rbh_collect_fsentries_test);
    suite_add_tcase(suite, tc_collect);

    TCase *tc_compare = tcase_create("compare_values");
    tcase_add_test(tc_compare, compare_values_success_test);
    tcase_add_test(tc_compare, compare_values_failure_test);
    suite_add_tcase(suite, tc_compare);

    TCase *tc_execute = tcase_create("pe_execute");
    tcase_add_test(tc_execute, rbh_pe_execute_empty_iter_test);
    suite_add_tcase(suite, tc_execute);

    TCase *tc_filter_matches = tcase_create("filter_matches_fsentry");
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_null_filter_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_equality_match_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_equality_no_match_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_greater_match_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_greater_no_match_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_missing_field_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_and_all_true_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_and_one_false_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_or_one_true_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_or_all_false_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_not_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_exists_present_test);
    tcase_add_test(tc_filter_matches,
                   rbh_filter_matches_fsentry_exists_absent_test);
    suite_add_tcase(suite, tc_filter_matches);

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
