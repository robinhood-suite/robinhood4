/* This file is part of the RobinHood Library
 * Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "robinhood/ringr.h"

#include "check-compat.h"

static void
get_page_size(void) __attribute__((constructor));

static long page_size;
static void
get_page_size(void)
{
    page_size = sysconf(_SC_PAGESIZE);
}

/* XXX: consider using getrandom(), which do the same here.
 * The syscall was introduced in ver 3.17 of the Linux kernel and its
 * support in glibc ver 2.25. Even if the syscall was backported to el7
 * kernel, it hasn't been to its glibc.
 * We can consider swapping once el7 systems are not targeted anymore.
 */
static void
random_read(char *buffer, size_t size)
{
    ssize_t read_bytes = 0;
    int fd;

    fd = open("/dev/urandom", O_RDONLY);
    ck_assert_int_gt(fd, 0);
    do {
        read_bytes += read(fd, buffer, size);
        ck_assert_int_ge(read_bytes, 0);
    } while (read_bytes < size);

    close(fd);
}

/*----------------------------------------------------------------------------*
 |                                 unit tests                                 |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                           rbh_ringr_new                            |
     *--------------------------------------------------------------------*/

START_TEST(rrn_hollow)
{
    errno = 0;
    ck_assert_ptr_null(rbh_ringr_new(0));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rrn_unaligned)
{
    errno = 0;
    ck_assert_ptr_null(rbh_ringr_new(1));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rrn_basic)
{
    struct rbh_ringr *ringr;

    ringr = rbh_ringr_new(page_size);

    ck_assert_ptr_nonnull(ringr);
    rbh_ringr_destroy(ringr);
}
END_TEST


    /*--------------------------------------------------------------------*
     |                           rbh_ringr_dup                            |
     *--------------------------------------------------------------------*/

START_TEST(rrd_once)
{
    struct rbh_ringr *duplicate;
    struct rbh_ringr *ringr;

    ringr = rbh_ringr_new(page_size);
    ck_assert_ptr_nonnull(ringr);

    duplicate = rbh_ringr_dup(ringr);
    ck_assert_ptr_nonnull(duplicate);

    rbh_ringr_destroy(duplicate);
    rbh_ringr_destroy(ringr);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                           rbh_ringr_peek                           |
     *--------------------------------------------------------------------*/

START_TEST(rrp_empty)
{
    struct rbh_ringr *ringr;
    size_t size;

    ringr = rbh_ringr_new(page_size);

    rbh_ringr_peek(ringr, &size);
    ck_assert_int_eq(size, 0);

    rbh_ringr_destroy(ringr);
}
END_TEST

START_TEST(rrp_reproduceable)
{
    struct rbh_ringr *ringr[2];
    size_t count[2];
    void *data[2];

    ringr[0] = rbh_ringr_new(page_size);
    ringr[1] = rbh_ringr_dup(ringr[0]);

    data[0] = rbh_ringr_peek(ringr[0], &count[0]);
    data[1] = rbh_ringr_peek(ringr[1], &count[1]);
    ck_assert_int_eq(count[0], count[1]);
    ck_assert_ptr_eq(data[0], data[1]);

    rbh_ringr_destroy(ringr[0]);
    rbh_ringr_destroy(ringr[1]);
}
END_TEST

START_TEST(rrp_some)
{
    const char *string = "abcdefghijklmno";
    struct rbh_ringr *ringr;
    size_t size;
    void *head;

    ringr = rbh_ringr_new(page_size);

    head = rbh_ringr_push(ringr, string, strlen(string) + 1);
    ck_assert_ptr_eq(rbh_ringr_peek(ringr, &size), head);
    ck_assert_int_eq(size, strlen(string) + 1);

    rbh_ringr_destroy(ringr);
}
END_TEST

START_TEST(rrp_full)
{
    struct rbh_ringr *ringr;
    char buffer[UCHAR_MAX + 1];
    size_t size;
    char *head;

    ck_assert_int_eq(page_size % (UCHAR_MAX + 1), 0);
    for (size_t i = 0; i < UCHAR_MAX + 1; i++)
        buffer[i] = i;

    ringr = rbh_ringr_new(page_size);

    head = rbh_ringr_push(ringr, buffer, UCHAR_MAX + 1);
    ck_assert_mem_eq(head, buffer, UCHAR_MAX + 1);

    for (size_t i = 1; i < page_size / (UCHAR_MAX + 1); i++) {
        void *data;

        data = rbh_ringr_push(ringr, buffer, UCHAR_MAX + 1);
        ck_assert_ptr_nonnull(data);
        ck_assert_mem_eq(data, buffer, UCHAR_MAX + 1);
    }

    errno = 0;
    ck_assert_ptr_null(rbh_ringr_push(ringr, buffer, UCHAR_MAX + 1));
    ck_assert_int_eq(errno, ENOBUFS);
    ck_assert_ptr_eq(head, rbh_ringr_peek(ringr, &size));
    ck_assert_int_eq(size, page_size);

    rbh_ringr_destroy(ringr);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                           rbh_ringr_ack                            |
     *--------------------------------------------------------------------*/

START_TEST(rra_single_reader)
{
    struct rbh_ringr *ringr;
    size_t size;
    char *head;

    ringr = rbh_ringr_new(page_size);

    head = rbh_ringr_push(ringr, "abcdefghijklmno", 16);
    rbh_ringr_ack(ringr, 8);
    ck_assert_ptr_eq(rbh_ringr_peek(ringr, &size), head + 8);
    ck_assert_int_eq(size, 8);
    rbh_ringr_ack(ringr, 8);
    ck_assert_ptr_eq(rbh_ringr_peek(ringr, &size), head + 16);
    ck_assert_int_eq(size, 0);

    rbh_ringr_destroy(ringr);
}
END_TEST

START_TEST(rra_multi_reader)
{
    struct rbh_ringr *ringr[2];
    char buffer[page_size];
    size_t size;
    char *head;

    ringr[0] = rbh_ringr_new(page_size);
    ringr[1] = rbh_ringr_dup(ringr[0]);

    random_read(buffer, page_size);
    /* Put 16 bytes in the ring */
    head = rbh_ringr_push(ringr[0], "abcdefghijklmno", 16);
    /* Ack them for the first reader */
    rbh_ringr_ack(ringr[0], 16);
    ck_assert_ptr_eq(rbh_ringr_peek(ringr[0], &size), head + 16);
    ck_assert_int_eq(size, 0);
    /* Check the ring is not empty */
    errno = 0;
    ck_assert_ptr_null(rbh_ringr_push(ringr[0], buffer, page_size - 15));
    ck_assert_int_eq(errno, ENOBUFS);
    /* Check the second reader still sees those bytes */
    ck_assert_ptr_eq(rbh_ringr_peek(ringr[1], &size), head);
    ck_assert_int_eq(size, 16);
    /* Check the empty space in the ring is the expected size */
    head = rbh_ringr_push(ringr[0], buffer, page_size - 16);
    ck_assert_mem_eq(head, buffer, page_size - 16);
    ck_assert_ptr_eq(rbh_ringr_peek(ringr[0], &size), head);
    /* Ack the whole ring for both readers */
    rbh_ringr_ack(ringr[0], page_size - 16);
    rbh_ringr_ack(ringr[1], page_size);
    rbh_ringr_peek(ringr[0], &size);
    ck_assert_int_eq(size, 0);
    rbh_ringr_peek(ringr[1], &size);
    ck_assert_int_eq(size, 0);

    rbh_ringr_destroy(ringr[0]);
    rbh_ringr_destroy(ringr[1]);
}
END_TEST

START_TEST(rra_multi_loop)
{
    struct rbh_ringr *duplicate;
    struct rbh_ringr *ringr;
    char buffer[page_size];
    size_t size;

    ringr = rbh_ringr_new(page_size);
    duplicate = rbh_ringr_dup(ringr);

    random_read(buffer, page_size);

    ck_assert_ptr_ne(rbh_ringr_push(ringr, buffer, page_size * 3 / 4), NULL);
    ck_assert_int_eq(rbh_ringr_ack(ringr, page_size / 2), 0);
    ck_assert_int_eq(rbh_ringr_ack(duplicate, page_size / 2), 0);

    ck_assert_ptr_ne(rbh_ringr_push(ringr, buffer, page_size / 2), NULL);
    ck_assert_int_eq(rbh_ringr_ack(ringr, page_size / 2 + 1), 0);
    ck_assert_int_eq(rbh_ringr_ack(duplicate, page_size / 2 - 1), 0);

    ck_assert_ptr_ne(rbh_ringr_push(ringr, buffer, page_size * 3 / 4 - 1), NULL);
    ck_assert_int_eq(rbh_ringr_ack(ringr, page_size - 2), 0);
    ck_assert_int_eq(rbh_ringr_ack(duplicate, page_size), 0);

    rbh_ringr_peek(ringr, &size);
    ck_assert_int_eq(size, 0);
    rbh_ringr_peek(duplicate, &size);
    ck_assert_int_eq(size, 0);

    rbh_ringr_destroy(ringr);
    rbh_ringr_destroy(duplicate);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("unit tests");

    tests = tcase_create("rbh_ringr_new");
    tcase_add_test(tests, rrn_hollow);
    tcase_add_test(tests, rrn_unaligned);
    tcase_add_test(tests, rrn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_ringr_dup");
    tcase_add_test(tests, rrd_once);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_ringr_peek");
    tcase_add_test(tests, rrp_empty);
    tcase_add_test(tests, rrp_reproduceable);
    tcase_add_test(tests, rrp_some);
    tcase_add_test(tests, rrp_full);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_ringr_ack");
    tcase_add_test(tests, rra_single_reader);
    tcase_add_test(tests, rra_multi_reader);
    tcase_add_test(tests, rra_multi_loop);

    suite_add_tcase(suite, tests);

    return suite;
}

int
main(void)
{
    int number_failed;
    SRunner *runner;

    runner = srunner_create(unit_suite());

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
