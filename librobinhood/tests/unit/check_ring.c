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
#include <unistd.h>

#include "robinhood/ring.h"

#include "check-compat.h"

/*----------------------------------------------------------------------------*
 |                                 unit tests                                 |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                           rbh_ring_new()                           |
     *--------------------------------------------------------------------*/

START_TEST(rrn_hollow)
{
    errno = 0;
    ck_assert_ptr_null(rbh_ring_new(0));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

static void
get_page_size(void) __attribute__((constructor));

static long page_size;
static void
get_page_size(void)
{
    page_size = sysconf(_SC_PAGESIZE);
}

START_TEST(rrn_unaligned)
{
    errno = 0;
    ck_assert_ptr_null(rbh_ring_new(page_size + 1));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rrn_basic)
{
    struct rbh_ring *ring;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    rbh_ring_destroy(ring);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_ring_push()                           |
     *--------------------------------------------------------------------*/

START_TEST(rrpu_none)
{
    struct rbh_ring *ring;
    void *data;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    data = rbh_ring_push(ring, NULL, 0);
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_eq(rbh_ring_push(ring, NULL, 0), data);

    rbh_ring_destroy(ring);
}
END_TEST

START_TEST(rrpu_more_than_possible)
{
    struct rbh_ring *ring;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    errno = 0;
    ck_assert_ptr_null(rbh_ring_push(ring, NULL, page_size + 1));
    ck_assert_int_eq(errno, EINVAL);

    rbh_ring_destroy(ring);
}
END_TEST

START_TEST(rrpu_more_than_available)
{
    struct rbh_ring *ring;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    ck_assert_ptr_nonnull(rbh_ring_push(ring, NULL, page_size));

    errno = 0;
    ck_assert_ptr_null(rbh_ring_push(ring, NULL, 1));
    ck_assert_int_eq(errno, ENOBUFS);

    rbh_ring_destroy(ring);
}
END_TEST

START_TEST(rrpu_some)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_ring *ring;
    void *data;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    data = rbh_ring_push(ring, STRING, sizeof(STRING));
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_ne(data, STRING);
    ck_assert_mem_eq(data, STRING, sizeof(STRING));

    rbh_ring_destroy(ring);
}
END_TEST

START_TEST(rrpu_full)
{
    struct rbh_ring *ring;
    void *buffer;
    void *data;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    buffer = calloc(1, page_size);
    ck_assert_ptr_nonnull(buffer);

    data = rbh_ring_push(ring, buffer, page_size);
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_ne(data, buffer);
    ck_assert_mem_eq(data, buffer, page_size);

    free(buffer);

    errno = 0;
    ck_assert_ptr_null(rbh_ring_push(ring, NULL, 1));
    ck_assert_int_eq(errno, ENOBUFS);

    rbh_ring_destroy(ring);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_ring_peek()                           |
     *--------------------------------------------------------------------*/

START_TEST(rrpe_empty)
{
    struct rbh_ring *ring;
    size_t size;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    rbh_ring_peek(ring, &size);
    ck_assert_uint_eq(size, 0);

    rbh_ring_destroy(ring);
}
END_TEST

START_TEST(rrpe_consistency)
{
    struct rbh_ring *ring;
    size_t size;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    ck_assert_ptr_eq(rbh_ring_push(ring, NULL, 0), rbh_ring_peek(ring, &size));
    ck_assert_uint_eq(size, 0);

    rbh_ring_destroy(ring);
}
END_TEST

START_TEST(rrpe_some)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_ring *ring;
    size_t size;
    void *data;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    data = rbh_ring_push(ring, STRING, sizeof(STRING));
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_eq(rbh_ring_peek(ring, &size), data);
    ck_assert_uint_eq(size, sizeof(STRING));
    ck_assert_mem_eq(data, STRING, size);

    rbh_ring_destroy(ring);
}
END_TEST

START_TEST(rrpe_full)
{
    struct rbh_ring *ring;
    char *buffer;
    size_t size;
    void *data;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    buffer = calloc(1, page_size);
    ck_assert_ptr_nonnull(buffer);

    data = rbh_ring_push(ring, buffer, page_size);
    ck_assert_ptr_nonnull(data);

    ck_assert_ptr_eq(rbh_ring_peek(ring, &size), data);
    ck_assert_uint_eq(size, page_size);
    ck_assert_mem_eq(data, buffer, page_size);

    free(buffer);
    rbh_ring_destroy(ring);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                           rbh_ring_pop()                           |
     *--------------------------------------------------------------------*/

START_TEST(rrpo_none)
{
    struct rbh_ring *ring;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    ck_assert_int_eq(rbh_ring_pop(ring, 0), 0);

    rbh_ring_destroy(ring);
}
END_TEST

START_TEST(rrpo_some)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_ring *ring;
    size_t size;
    void *data;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    data = rbh_ring_push(ring, STRING, sizeof(STRING));
    ck_assert_ptr_nonnull(data);

    ck_assert_int_eq(rbh_ring_pop(ring, sizeof(STRING)), 0);

    ck_assert_ptr_eq(rbh_ring_peek(ring, &size), data + sizeof(STRING));
    ck_assert_uint_eq(size, 0);

    rbh_ring_destroy(ring);
}
END_TEST

START_TEST(rrpo_too_much)
{
    struct rbh_ring *ring;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    errno = 0;
    ck_assert_int_eq(rbh_ring_pop(ring, 1), -1);
    ck_assert_int_eq(errno, EINVAL);

    rbh_ring_destroy(ring);
}
END_TEST

START_TEST(rrpo_full)
{
    struct rbh_ring *ring;
    size_t size;
    void *data;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    data = rbh_ring_push(ring, NULL, page_size);
    ck_assert_ptr_nonnull(data);
    ck_assert_int_eq(rbh_ring_pop(ring, page_size), 0);
    ck_assert_ptr_eq(rbh_ring_peek(ring, &size), data);
    ck_assert_uint_eq(size, 0);

    rbh_ring_destroy(ring);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("unit tests");

    tests = tcase_create("rbh_ring_new");
    tcase_add_test(tests, rrn_hollow);
    tcase_add_test(tests, rrn_unaligned);
    tcase_add_test(tests, rrn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_ring_push");
    tcase_add_test(tests, rrpu_none);
    tcase_add_test(tests, rrpu_more_than_possible);
    tcase_add_test(tests, rrpu_more_than_available);
    tcase_add_test(tests, rrpu_some);
    tcase_add_test(tests, rrpu_full);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_ring_peek");
    tcase_add_test(tests, rrpe_empty);
    tcase_add_test(tests, rrpe_consistency);
    tcase_add_test(tests, rrpe_some);
    tcase_add_test(tests, rrpe_full);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_ring_pop");
    tcase_add_test(tests, rrpo_none);
    tcase_add_test(tests, rrpo_some);
    tcase_add_test(tests, rrpo_too_much);
    tcase_add_test(tests, rrpo_full);

    suite_add_tcase(suite, tests);

    return suite;
}

/*----------------------------------------------------------------------------*
 |                             integration tests                              |
 *----------------------------------------------------------------------------*/

START_TEST(one_by_one)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_ring *ring;
    size_t size;
    void *data;

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    /* put bytes one by one */
    for (size_t i = 0; i < sizeof(STRING); i++)
        ck_assert_ptr_nonnull(rbh_ring_push(ring, &STRING[i], 1));

    /* check the content of the ring */
    data = rbh_ring_peek(ring, &size);
    ck_assert_uint_eq(size, sizeof(STRING));
    ck_assert_mem_eq(data, STRING, sizeof(STRING));

    /* ack bytes one by one */
    for (size_t i = 1; i <= sizeof(STRING); i++) {
        ck_assert_int_eq(rbh_ring_pop(ring, 1), 0);
        ck_assert_ptr_eq(rbh_ring_peek(ring, &size), data + i);
        ck_assert_uint_eq(size, sizeof(STRING) - i);
    }

    /* check the ring is empty */
    errno = 0;
    ck_assert_int_eq(rbh_ring_pop(ring, 1), -1);
    ck_assert_int_eq(errno, EINVAL);

    rbh_ring_destroy(ring);
}
END_TEST

START_TEST(chunk_by_chunk_until_full)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_ring *ring;
    size_t size;
    char *data;

    ck_assert_uint_eq(page_size % sizeof(STRING), 0);

    ring = rbh_ring_new(page_size);
    ck_assert_ptr_nonnull(ring);

    /* Fill up the ring */
    for (size_t i = 0; i < page_size / sizeof(STRING); i++)
        ck_assert_ptr_nonnull(rbh_ring_push(ring, STRING, sizeof(STRING)));

    /* Check the ring appears full */
    data = rbh_ring_peek(ring, &size);
    ck_assert_ptr_nonnull(data);
    ck_assert_uint_eq(size, page_size);

    /* Check the ring _is_ full */
    errno = 0;
    ck_assert_ptr_null(rbh_ring_push(ring, STRING, sizeof(STRING)));
    ck_assert_int_eq(errno, ENOBUFS);

    /* Check the content of the ring */
    for (size_t i = 0; i < page_size / sizeof(STRING);
         i++, data += sizeof(STRING)) {
        ck_assert_mem_eq(data, STRING, sizeof(STRING));
        ck_assert_int_eq(rbh_ring_pop(ring, sizeof(STRING)), 0);
    }

    /* Check the ring's head is back to its initial position */
    ck_assert_ptr_eq(rbh_ring_peek(ring, &size), data - page_size);
    ck_assert_uint_eq(size, 0);

    rbh_ring_destroy(ring);
}
END_TEST

static Suite *
integration_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("integration tests");

    tests = tcase_create("examples");
    tcase_add_test(tests, one_by_one);
    tcase_add_test(tests, chunk_by_chunk_until_full);

    suite_add_tcase(suite, tests);

    return suite;
}

int
main(void)
{
    int number_failed;
    SRunner *runner;

    runner = srunner_create(unit_suite());
    srunner_add_suite(runner, integration_suite());

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
