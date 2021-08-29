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

#include "robinhood/fsevent.h"
#include "robinhood/statx.h"
#ifndef HAVE_STATX
# include "robinhood/statx-compat.h"
#endif

#include "check-compat.h"
#include "check_macros.h"

#define ck_assert_fsevent_eq(X, Y) do { \
    ck_assert_int_eq((X)->type, (Y)->type); \
    ck_assert_id_eq(&(X)->id, &(Y)->id); \
    ck_assert_value_map_eq(&(X)->xattrs, &(Y)->xattrs); \
    switch ((X)->type) { \
    case RBH_FET_UPSERT: \
        if ((X)->upsert.statx == NULL) { \
            ck_assert_ptr_eq((X)->upsert.statx, (Y)->upsert.statx); \
        } else { \
            ck_assert_ptr_nonnull((Y)->upsert.statx); \
            ck_assert_mem_eq((X)->upsert.statx, (Y)->upsert.statx, \
                             sizeof(struct statx)); \
        } \
        ck_assert_pstr_eq((X)->upsert.symlink, (Y)->upsert.symlink); \
        break; \
    case RBH_FET_XATTR: \
        if ((X)->ns.parent_id == NULL) { \
            ck_assert_ptr_null((X)->ns.name); \
            ck_assert_ptr_eq((X)->ns.parent_id, (Y)->ns.parent_id); \
            ck_assert_ptr_eq((X)->ns.name, (Y)->ns.name); \
            break; \
        } \
        ck_assert_ptr_nonnull((X)->ns.name); \
        ck_assert_ptr_nonnull((Y)->ns.parent_id); \
        ck_assert_ptr_nonnull((Y)->ns.name); \
    case RBH_FET_LINK: \
    case RBH_FET_UNLINK: \
        ck_assert_id_eq((X)->link.parent_id, (Y)->link.parent_id); \
        ck_assert_str_eq((X)->link.name, (Y)->link.name); \
        break; \
    case RBH_FET_DELETE: \
        break; \
    default: \
        ck_abort_msg("unknown fsevent type %i", (X)->type); \
        break; \
    } \
} while (0)

/*----------------------------------------------------------------------------*
 |                          rbh_fsevent_upsert_new()                          |
 *----------------------------------------------------------------------------*/

START_TEST(rfupn_basic)
{
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_UPSERT,
        .id = {
            .data = "abcdefg",
            .size = 8,
        },
        .upsert.statx = NULL,
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_upsert_new(&FSEVENT.id, NULL, NULL, NULL);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);
    ck_assert_ptr_ne(fsevent->id.data, FSEVENT.id.data);

    free(fsevent);
}
END_TEST

START_TEST(rfupn_xattrs)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    const struct rbh_value_pair PAIR = {
        .key = "hijklmn",
        .value = &VALUE,
    };
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_UPSERT,
        .id = {
            .data = "opqrstu",
            .size = 8,
        },
        .xattrs = {
            .pairs = &PAIR,
            .count = 1,
        },
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_upsert_new(&FSEVENT.id, &FSEVENT.xattrs, NULL, NULL);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);

    free(fsevent);
}
END_TEST

START_TEST(rfupn_xattrs_misaligned)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_UINT32,
        .uint32 = 0,
    };
    const struct rbh_value_pair PAIR = {
        .key = "hijklmn",
        .value = &VALUE,
    };
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_UPSERT,
        .id = {
            .data = "opqrstu",
            .size = 7, /* not aligned */
        },
        .xattrs = {
            .pairs = &PAIR,
            .count = 1,
        },
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_upsert_new(&FSEVENT.id, &FSEVENT.xattrs, NULL, NULL);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);

    free(fsevent);
}
END_TEST

START_TEST(rfupn_statx)
{
    const struct statx STATX = {
        .stx_mask = RBH_STATX_UID,
        .stx_uid = 0,
    };
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_UPSERT,
        .id = {
            .data = "abcdefg",
            .size = 8,
        },
        .upsert.statx = &STATX,
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_upsert_new(&FSEVENT.id, NULL, &STATX, NULL);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);
    ck_assert_ptr_ne(fsevent->upsert.statx, &STATX);

    free(fsevent);
}
END_TEST

START_TEST(rfupn_statx_misaligned)
{
    const struct statx STATX = {
        .stx_mask = RBH_STATX_UID,
        .stx_uid = 0,
    };
    const struct rbh_value_pair PAIR = {
        .key = "hijklm", /* strlen(key) + 1 == 7 (not aligned) */
        .value = NULL,
    };
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_UPSERT,
        .id = {
            .data = "abcdefg",
            .size = 8,
        },
        .xattrs = {
            .pairs = &PAIR,
            .count = 1,
        },
        .upsert.statx = &STATX,
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_upsert_new(&FSEVENT.id, &FSEVENT.xattrs, &STATX,
                                     NULL);
    ck_assert_ptr_nonnull(fsevent);

    /* Access a member of the misaligned struct to trigger the sanitizer */
    ck_assert_int_eq(fsevent->upsert.statx->stx_mask, STATX.stx_mask);
    ck_assert_fsevent_eq(fsevent, &FSEVENT);
    ck_assert_ptr_ne(fsevent->upsert.statx, &STATX);

    free(fsevent);
}
END_TEST

START_TEST(rfupn_symlink)
{
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_UPSERT,
        .id = {
            .data ="abcdefg",
            .size = 8,
        },
        .upsert.symlink = "hijklmn",
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_upsert_new(&FSEVENT.id, NULL, NULL,
                                     FSEVENT.upsert.symlink);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);
    ck_assert_ptr_ne(fsevent->upsert.symlink, FSEVENT.upsert.symlink);

    free(fsevent);
}
END_TEST

START_TEST(rfupn_symlink_not_a_symlink)
{
    const struct rbh_id ID = {
        .data ="abcdefg",
        .size = 8,
    };
    const struct statx STATX = {
        .stx_mask = RBH_STATX_TYPE,
        .stx_mode = S_IFREG,
    };
    const char SYMLINK[] = "hijklmn";
    struct rbh_fsevent *fsevent;

    errno = 0;
    fsevent = rbh_fsevent_upsert_new(&ID, NULL, &STATX, SYMLINK);
    ck_assert_ptr_null(fsevent);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfupn_all)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    const struct rbh_value_pair PAIR = {
        .key = "hijklmn",
        .value = &VALUE,
    };
    const struct statx STATX = {
        .stx_mask = RBH_STATX_TYPE,
        .stx_mode = S_IFLNK,
    };
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_UPSERT,
        .id = {
            .data = "opqrstu",
            .size = 8,
        },
        .xattrs = {
            .pairs = &PAIR,
            .count = 1,
        },
        .upsert = {
            .statx = &STATX,
            .symlink = "vwxyzab",
        },
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_upsert_new(&FSEVENT.id, &FSEVENT.xattrs, &STATX,
                                     FSEVENT.upsert.symlink);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);

    free(fsevent);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_fsevent_link_new()                           |
 *----------------------------------------------------------------------------*/

START_TEST(rfln_basic)
{
    const struct rbh_id PARENT_ID = {
        .data = "abcdefg",
        .size = 8,
    };
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_LINK,
        .id = {
            .data = "hijklmn",
            .size = 8,
        },
        .link = {
            .parent_id = &PARENT_ID,
            .name = "opqrstu",
        },
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_link_new(&FSEVENT.id, NULL, &PARENT_ID,
                                   FSEVENT.link.name);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);
    ck_assert_ptr_ne(fsevent->link.parent_id->data, PARENT_ID.data);
    ck_assert_ptr_ne(fsevent->link.name, FSEVENT.link.name);

    free(fsevent);
}
END_TEST

START_TEST(rfln_null_parent_id)
{
    const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };

    errno = 0;
    ck_assert_ptr_null(rbh_fsevent_link_new(&ID, NULL, NULL, NULL));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfln_null_name)
{
    const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };
    const struct rbh_id PARENT_ID = {
        .data = "hijklmn",
        .size = 8,
    };

    errno = 0;
    ck_assert_ptr_null(rbh_fsevent_link_new(&ID, NULL, &PARENT_ID, NULL));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfln_parent_id_misaligned)
{
    const struct rbh_id PARENT_ID = {
        .data = "abcdefg",
        .size = 8,
    };
    const struct rbh_value_pair PAIR = {
        .key = "hijklm", /* strlen(key) + 1 == 7 (not aligned) */
        .value = NULL,
    };
    const struct rbh_fsevent FSEVENT = {
        .id = {
            .data = "nopqrst",
            .size = 8,
        },
        .xattrs = {
            .pairs = &PAIR,
            .count = 1,
        },
        .link = {
            .parent_id = &PARENT_ID,
            .name = "uvwxyza",
        },
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_link_new(&FSEVENT.id, &FSEVENT.xattrs,
                                   FSEVENT.link.parent_id, FSEVENT.link.name);
    ck_assert_ptr_nonnull(fsevent);

    free(fsevent);
}
END_TEST

START_TEST(rfln_xattrs)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    const struct rbh_value_pair PAIR = {
        .key = "hijklmn",
        .value = &VALUE,
    };
    const struct rbh_id PARENT_ID = {
        .data = "opqrstu",
        .size = 8,
    };
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_LINK,
        .id = {
            .data = "vwxyzab",
            .size = 8,
        },
        .xattrs = {
            .pairs = &PAIR,
            .count = 1,
        },
        .link = {
            .parent_id = &PARENT_ID,
            .name = "cdefghi",
        }
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_link_new(&FSEVENT.id, &FSEVENT.xattrs, &PARENT_ID,
                                   FSEVENT.link.name);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);

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
    const struct rbh_id PARENT_ID = {
        .data = "abcdefg",
        .size = 8,
    };
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_UNLINK,
        .id = {
            .data = "hijklmn",
            .size = 8,
        },
        .link = {
            .parent_id = &PARENT_ID,
            .name = "opqrstu",
        },
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_unlink_new(&FSEVENT.id, &PARENT_ID,
                                     FSEVENT.link.name);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);

    free(fsevent);
}
END_TEST

START_TEST(rfuln_null_parent_id)
{
    const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };

    errno = 0;
    ck_assert_ptr_null(rbh_fsevent_link_new(&ID, NULL, NULL, NULL));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfuln_null_name)
{
    const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };
    const struct rbh_id PARENT_ID = {
        .data = "hijklmn",
        .size = 8,
    };

    errno = 0;
    ck_assert_ptr_null(rbh_fsevent_link_new(&ID, NULL, &PARENT_ID, NULL));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                          rbh_fsevent_delete_new()                          |
 *----------------------------------------------------------------------------*/

START_TEST(rfdn_basic)
{
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_DELETE,
        .id = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_delete_new(&FSEVENT.id);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);

    free(fsevent);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                          rbh_fsevent_xattr_new()                           |
 *----------------------------------------------------------------------------*/

START_TEST(rfxn_basic)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    const struct rbh_value_pair PAIR = {
        .key = "hijklmn",
        .value = &VALUE,
    };
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_XATTR,
        .id = {
            .data = "opqrstu",
            .size = 8,
        },
        .xattrs = {
            .pairs = &PAIR,
            .count = 1,
        },
        .ns = {
            .parent_id = NULL,
            .name = NULL,
        },
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_xattr_new(&FSEVENT.id, &FSEVENT.xattrs);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);

    free(fsevent);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                         rbh_fsevent_ns_xattr_new()                         |
 *----------------------------------------------------------------------------*/

START_TEST(rfnxn_basic)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    const struct rbh_value_pair PAIR = {
        .key = "hijklmn",
        .value = &VALUE,
    };
    const struct rbh_id PARENT_ID = {
        .data = "opqrstu",
        .size = 8,
    };
    const struct rbh_fsevent FSEVENT = {
        .type = RBH_FET_XATTR,
        .id = {
            .data = "vwxyzab",
            .size = 8,
        },
        .xattrs = {
            .pairs = &PAIR,
            .count = 1,
        },
        .ns = {
            .parent_id = &PARENT_ID,
            .name = "cdefghi",
        },
    };
    struct rbh_fsevent *fsevent;

    fsevent = rbh_fsevent_ns_xattr_new(&FSEVENT.id, &FSEVENT.xattrs, &PARENT_ID,
                                       FSEVENT.ns.name);
    ck_assert_ptr_nonnull(fsevent);

    ck_assert_fsevent_eq(fsevent, &FSEVENT);

    free(fsevent);
}
END_TEST

START_TEST(rfnxn_null_parent_id)
{
    const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "hijklmn",
            .size = 8,
        },
    };
    const struct rbh_value_pair PAIR = {
        .key = "opqrstu",
        .value = &VALUE,
    };
    const struct rbh_value_map XATTRS = {
        .pairs = &PAIR,
        .count = 1,
    };

    errno = 0;
    ck_assert_ptr_null(rbh_fsevent_ns_xattr_new(&ID, &XATTRS, NULL, NULL));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfnxn_null_name)
{
    const struct rbh_id ID = {
        .data = "abcdefg",
        .size = 8,
    };
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "hijklmn",
            .size = 8,
        },
    };
    const struct rbh_value_pair PAIR = {
        .key = "opqrstu",
        .value = &VALUE,
    };
    const struct rbh_value_map XATTRS = {
        .pairs = &PAIR,
        .count = 1,
    };
    const struct rbh_id PARENT_ID = {
        .data = "vwxyzab",
        .size = 8,
    };

    errno = 0;
    ck_assert_ptr_null(
            rbh_fsevent_ns_xattr_new(&ID, &XATTRS, &PARENT_ID, NULL)
            );
    ck_assert_int_eq(errno, EINVAL);
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
    tcase_add_test(tests, rfupn_xattrs);
    tcase_add_test(tests, rfupn_xattrs_misaligned);
    tcase_add_test(tests, rfupn_statx);
    tcase_add_test(tests, rfupn_statx_misaligned);
    tcase_add_test(tests, rfupn_symlink);
    tcase_add_test(tests, rfupn_symlink_not_a_symlink);
    tcase_add_test(tests, rfupn_all);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_fsevent_link_new()");
    tcase_add_test(tests, rfln_basic);
    tcase_add_test(tests, rfln_null_parent_id);
    tcase_add_test(tests, rfln_null_name);
    tcase_add_test(tests, rfln_parent_id_misaligned);
    tcase_add_test(tests, rfln_xattrs);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_fsevent_unlink_new()");
    tcase_add_test(tests, rfuln_basic);
    tcase_add_test(tests, rfuln_null_parent_id);
    tcase_add_test(tests, rfuln_null_name);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_fsevent_delete_new()");
    tcase_add_test(tests, rfdn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_fsevent_xattr_new()");
    tcase_add_test(tests, rfxn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_fsevent_xattr_new()");
    tcase_add_test(tests, rfnxn_basic);
    tcase_add_test(tests, rfnxn_null_parent_id);
    tcase_add_test(tests, rfnxn_null_name);

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
