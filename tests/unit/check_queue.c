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
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "robinhood/queue.h"

#include "check-compat.h"

/*----------------------------------------------------------------------------*
 |                                 unit tests                                 |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                          rbh_queue_new()                           |
     *--------------------------------------------------------------------*/

START_TEST(rqn_hollow)
{
    errno = 0;
    ck_assert_ptr_null(rbh_queue_new(0));
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

START_TEST(rqn_basic)
{
    struct rbh_queue *queue;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    rbh_queue_destroy(queue);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_queue_push()                          |
     *--------------------------------------------------------------------*/

START_TEST(rqpu_none)
{
    struct rbh_queue *queue;
    void *data;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    data = rbh_queue_push(queue, NULL, 0);
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_eq(rbh_queue_push(queue, NULL, 0), data);

    rbh_queue_destroy(queue);
}
END_TEST

START_TEST(rqpu_more_than_possible)
{
    struct rbh_queue *queue;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    errno = 0;
    ck_assert_ptr_null(rbh_queue_push(queue, NULL, page_size + 1));
    ck_assert_int_eq(errno, EINVAL);

    rbh_queue_destroy(queue);
}
END_TEST

START_TEST(rqpu_some)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_queue *queue;
    void *data;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    data = rbh_queue_push(queue, STRING, sizeof(STRING));
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_ne(data, STRING);
    ck_assert_mem_eq(data, STRING, sizeof(STRING));

    rbh_queue_destroy(queue);
}
END_TEST

START_TEST(rqpu_full_twice)
{
    struct rbh_queue *queue;
    void *buffer;
    void *data;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    buffer = calloc(1, page_size);
    ck_assert_ptr_nonnull(buffer);

    data = rbh_queue_push(queue, buffer, page_size);
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_ne(data, buffer);
    ck_assert_mem_eq(data, buffer, page_size);

    data = rbh_queue_push(queue, buffer, page_size);
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_ne(data, buffer);
    ck_assert_mem_eq(data, buffer, page_size);

    free(buffer);
    rbh_queue_destroy(queue);
}
END_TEST

START_TEST(rqpu_reusing_rings)
{
    struct rbh_queue *queue;
    void *data;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    data = rbh_queue_push(queue, NULL, page_size);
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_nonnull(rbh_queue_push(queue, NULL, page_size));
    ck_assert_int_eq(rbh_queue_pop(queue, page_size), 0);
    ck_assert_ptr_eq(rbh_queue_push(queue, NULL, page_size), data);

    rbh_queue_destroy(queue);
}
END_TEST

/* At the time this test is written, a queue is a collections of ring buffers.
 * Those ring buffers are stored in an array. When the queue needs to manage
 * more ring buffers than there are slots in this array, the array's size is
 * doubled.
 * A queue is a FIFO container, that means that the first rings to be filled are
 * also the first ones to be emptied. That, in turn, means that the first slots
 * in the array that stores rings may be unused at the time it is enlarged.
 * In this case, rather than doubling its size, rbh_queue_push() will just
 * left-align the array's data.
 * At the time this is written, the heuristic to choose whether to realloc the
 * array or left-align it is:
 *   If the array is more than halfway empty, left-align it, otherwise double
 *   its size.
 *
 * This optimization is internal and nothing in the queue's API allows anyone to
 * figure out when it happens or even _if_ it happens. Thus, it is the
 * developers' responsibility to ensure the following test stays relevant, or is
 * removed.
 */
START_TEST(rqpu_moving_rings_optimization)
{
    struct rbh_queue *queue;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    ck_assert_ptr_nonnull(rbh_queue_push(queue, NULL, page_size));
    ck_assert_ptr_nonnull(rbh_queue_push(queue, NULL, page_size));
    ck_assert_ptr_nonnull(rbh_queue_push(queue, NULL, page_size));
    ck_assert_ptr_nonnull(rbh_queue_push(queue, NULL, page_size));
    /* The queue's internal array of ring now has 4 slots, all of them used */

    /* Pop data from the first 3 rings */
    ck_assert_int_eq(rbh_queue_pop(queue, page_size), 0);
    ck_assert_int_eq(rbh_queue_pop(queue, page_size), 0);
    ck_assert_int_eq(rbh_queue_pop(queue, page_size), 0);
    /* Now the array is 3/4 empty (we need it more than halfway empty), but
     * still, there are no available slots */

    /* Push another page to trigger the optimization */
    ck_assert_ptr_nonnull(rbh_queue_push(queue, NULL, page_size));

    /* And that's about it... This test is mostly about coverage */
    rbh_queue_destroy(queue);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_queue_peek()                          |
     *--------------------------------------------------------------------*/

START_TEST(rqpe_empty)
{
    struct rbh_queue *queue;
    size_t size;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    rbh_queue_peek(queue, &size);
    ck_assert_uint_eq(size, 0);

    rbh_queue_destroy(queue);
}
END_TEST

START_TEST(rqpe_consistency)
{
    struct rbh_queue *queue;
    size_t size;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    ck_assert_ptr_eq(rbh_queue_push(queue, NULL, 0),
                     rbh_queue_peek(queue, &size));
    ck_assert_uint_eq(size, 0);

    rbh_queue_destroy(queue);
}
END_TEST

START_TEST(rqpe_some)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_queue *queue;
    size_t size;
    void *data;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    data = rbh_queue_push(queue, STRING, sizeof(STRING));
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_eq(rbh_queue_peek(queue, &size), data);
    ck_assert_uint_eq(size, sizeof(STRING));
    ck_assert_mem_eq(data, STRING, size);

    rbh_queue_destroy(queue);
}
END_TEST

START_TEST(rqpe_full_twice)
{
    struct rbh_queue *queue;
    void *buffer;
    size_t size;
    void *data;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    buffer = calloc(1, page_size);
    ck_assert_ptr_nonnull(buffer);

    data = rbh_queue_push(queue, buffer, page_size);
    ck_assert_ptr_nonnull(data);

    ck_assert_ptr_nonnull(rbh_queue_push(queue, buffer, page_size));

    ck_assert_ptr_eq(rbh_queue_peek(queue, &size), data);
    ck_assert_uint_eq(size, page_size);
    ck_assert_mem_eq(data, buffer, page_size);

    free(buffer);
    rbh_queue_destroy(queue);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_queue_pop()                           |
     *--------------------------------------------------------------------*/

START_TEST(rqpo_too_much)
{
    struct rbh_queue *queue;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    errno = 0;
    ck_assert_int_eq(rbh_queue_pop(queue, 1), -1);
    ck_assert_int_eq(errno, EINVAL);

    rbh_queue_destroy(queue);
}
END_TEST

START_TEST(rqpo_after_full_twice)
{
    struct rbh_queue *queue;
    size_t size;

    queue = rbh_queue_new(page_size);
    ck_assert_ptr_nonnull(queue);

    ck_assert_ptr_nonnull(rbh_queue_push(queue, NULL, page_size));
    ck_assert_ptr_nonnull(rbh_queue_push(queue, NULL, 1));

    ck_assert_ptr_nonnull(rbh_queue_peek(queue, &size));
    ck_assert_uint_eq(size, page_size);
    ck_assert_int_eq(rbh_queue_pop(queue, page_size), 0);

    ck_assert_ptr_nonnull(rbh_queue_peek(queue, &size));
    ck_assert_uint_eq(size, 1);
    ck_assert_int_eq(rbh_queue_pop(queue, 1), 0);

    rbh_queue_destroy(queue);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("unit tests");

    tests = tcase_create("rbh_queue_new()");
    tcase_add_test(tests, rqn_hollow);
    tcase_add_test(tests, rqn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_queue_push()");
    tcase_add_test(tests, rqpu_none);
    tcase_add_test(tests, rqpu_more_than_possible);
    tcase_add_test(tests, rqpu_some);
    tcase_add_test(tests, rqpu_full_twice);
    tcase_add_test(tests, rqpu_reusing_rings);
    tcase_add_test(tests, rqpu_moving_rings_optimization);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_queue_peek()");
    tcase_add_test(tests, rqpe_empty);
    tcase_add_test(tests, rqpe_consistency);
    tcase_add_test(tests, rqpe_some);
    tcase_add_test(tests, rqpe_full_twice);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_queue_pop()");
    tcase_add_test(tests, rqpo_too_much);
    tcase_add_test(tests, rqpo_after_full_twice);

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
