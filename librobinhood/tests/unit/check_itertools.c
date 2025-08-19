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
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "check-compat.h"
#include "robinhood/itertools.h"
#include "robinhood/itertools.h"

/*----------------------------------------------------------------------------*
 |                              rbh_iter_array()                              |
 *----------------------------------------------------------------------------*/

START_TEST(ria_basic)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_iterator *letters;

    letters = rbh_iter_array(STRING, sizeof(*STRING), sizeof(STRING), NULL);
    ck_assert_ptr_nonnull(letters);

    for (size_t i = 0; i < sizeof(STRING); i++)
        ck_assert_mem_eq(rbh_iter_next(letters), &STRING[i], sizeof(*STRING));

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(letters));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(letters);
}
END_TEST

static bool ria_freed = false;

void
custom_free(void *arg)
{
    free(arg);
    ria_freed = true;
}

START_TEST(ria_free)
{
    const char LETTERS[] = "abcdefghijklmno";
    struct rbh_iterator *letters;
    char *STRING;

    STRING = malloc(sizeof(LETTERS) * sizeof(*STRING));
    ck_assert_ptr_nonnull(STRING);

    for (int i = 0; i < sizeof(LETTERS); i++)
        STRING[i] = LETTERS[i];

    letters = rbh_iter_array(STRING, sizeof(*STRING), sizeof(STRING),
                             custom_free);
    ck_assert_ptr_nonnull(letters);

    rbh_iter_destroy(letters);
    ck_assert_int_eq(ria_freed, 1);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_iter_chunkify()                             |
 *----------------------------------------------------------------------------*/

START_TEST(richu_basic)
{
    const char STRING[] = "abcdefghijklmno";
    const size_t CHUNK_SIZE = 4;
    struct rbh_mut_iterator *chunks;
    struct rbh_iterator *letters;

    ck_assert_uint_eq(sizeof(STRING) % CHUNK_SIZE, 0);

    letters = rbh_iter_array(STRING, sizeof(*STRING), sizeof(STRING), NULL);
    ck_assert_ptr_nonnull(letters);

    chunks = rbh_iter_chunkify(letters, CHUNK_SIZE);
    ck_assert_ptr_nonnull(chunks);

    for (size_t i = 0; i < sizeof(STRING) / CHUNK_SIZE; i++) {
        struct rbh_iterator *chunk;

        chunk = rbh_mut_iter_next(chunks);
        ck_assert_ptr_nonnull(chunk);

        for (size_t j = 0; j < CHUNK_SIZE; j++)
            ck_assert_mem_eq(rbh_iter_next(chunk), &STRING[i * CHUNK_SIZE + j],
                             sizeof(*STRING));

        errno = 0;
        ck_assert_ptr_null(rbh_iter_next(chunk));
        ck_assert_int_eq(errno, ENODATA);

        rbh_iter_destroy(chunk);
    }

    errno = 0;
    ck_assert_ptr_null(rbh_mut_iter_next(chunks));
    ck_assert_int_eq(errno, ENODATA);

    rbh_mut_iter_destroy(chunks);
}
END_TEST

static const void *
null_iter_next(void *iterator)
{
    (void) iterator;
    return NULL;
}

static void
null_iter_destroy(void *iterator)
{
    (void) iterator;
    ;
}

static const struct rbh_iterator_operations NULL_ITER_OPS = {
    .next = null_iter_next,
    .destroy = null_iter_destroy,
};

static const struct rbh_iterator NULL_ITER = {
    .ops = &NULL_ITER_OPS,
};

START_TEST(richu_with_null_elements)
{
    struct rbh_iterator nulls = NULL_ITER;
    struct rbh_mut_iterator *chunks;
    struct rbh_iterator *chunk;
    const size_t CHUNK_SIZE = 3;

    chunks = rbh_iter_chunkify(&nulls, CHUNK_SIZE);
    ck_assert_ptr_nonnull(chunks);

    chunk = rbh_mut_iter_next(chunks);
    ck_assert_ptr_nonnull(chunk);

    for (size_t i = 0; i < CHUNK_SIZE; i++) {
        errno = 0;
        ck_assert_ptr_null(rbh_iter_next(chunk));
        ck_assert_int_eq(errno, 0);
    }

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(chunk));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(chunk);
    rbh_mut_iter_destroy(chunks);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                               rbh_iter_tee()                               |
 *----------------------------------------------------------------------------*/

START_TEST(rit_basic)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_iterator *tees[2];
    struct rbh_iterator *letters;

    letters = rbh_iter_array(STRING, sizeof(*STRING), sizeof(STRING), NULL);
    ck_assert_ptr_nonnull(letters);

    ck_assert_int_eq(rbh_iter_tee(letters, tees), 0);

    for (size_t i = 0; i < sizeof(STRING); i++)
        ck_assert_mem_eq(rbh_iter_next(tees[0]), &STRING[i], sizeof(*STRING));

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(tees[0]));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(tees[0]);

    for (size_t i = 0; i < sizeof(STRING); i++)
        ck_assert_mem_eq(rbh_iter_next(tees[1]), &STRING[i], sizeof(*STRING));

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(tees[1]));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(tees[1]);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              rbh_iter_chain()                              |
 *----------------------------------------------------------------------------*/

START_TEST(richa_basic)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_iterator *chain;
    struct rbh_iterator *start;
    struct rbh_iterator *end;

    start = rbh_iter_array(STRING, sizeof(*STRING), sizeof(STRING) / 2, NULL);
    ck_assert_ptr_nonnull(start);

    end = rbh_iter_array(STRING + sizeof(STRING) / 2, sizeof(*STRING),
                         (sizeof(STRING) + 1) / 2, NULL);
    ck_assert_ptr_nonnull(end);

    chain = rbh_iter_chain(start, end);
    ck_assert_ptr_nonnull(chain);

    for (size_t i = 0; i < sizeof(STRING); i++)
        ck_assert_mem_eq(rbh_iter_next(chain), &STRING[i], sizeof(*STRING));

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(chain));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(chain);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_iter_constify()                             |
 *----------------------------------------------------------------------------*/

struct ascii_iterator {
    struct rbh_mut_iterator iterator;
    unsigned char c;
};

static void *
ascii_iter_next(void *iterator)
{
    struct ascii_iterator *ascii = iterator;
    char *c;

    c = malloc(sizeof(*c));
    if (c == NULL)
        return NULL;

    *c = ascii->c++;
    return c;
}

static void
ascii_iter_destroy(void *iterator)
{
    (void) iterator;
    return;
}

static const struct rbh_mut_iterator_operations ASCII_ITER_OPS = {
    .next = ascii_iter_next,
    .destroy = ascii_iter_destroy,
};

static const struct rbh_mut_iterator ASCII_ITERATOR = {
    .ops = &ASCII_ITER_OPS,
};

/* We have to rely on libasan to test that memory is properly deallocated */
START_TEST(rico_basic)
{
    const char STRING[] = "abcdefghijklmno";
    struct ascii_iterator _ascii;
    struct rbh_iterator *ascii;

    _ascii.iterator = ASCII_ITERATOR;
    _ascii.c = 'a';

    ascii = rbh_iter_constify(&_ascii.iterator);
    ck_assert_ptr_nonnull(ascii);

    for (size_t i = 0; i < sizeof(STRING) - 1; i++)
        ck_assert_mem_eq(rbh_iter_next(ascii), &STRING[i], sizeof(*STRING));

    rbh_iter_destroy(ascii);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              rbh_iter_ring()                               |
 *----------------------------------------------------------------------------*/

static size_t pagesize;

static void __attribute__((constructor))
pagesize_init(void)
{
    pagesize = sysconf(_SC_PAGESIZE);
}

START_TEST(rir_basic)
{
    const char STRING[] = "abcdefghijklmno";
    struct rbh_iterator *iter;
    struct rbh_ring *ring;
    size_t readable;

    ring = rbh_ring_new(pagesize);
    ck_assert_ptr_nonnull(ring);

    ck_assert_ptr_nonnull(rbh_ring_push(ring, STRING, sizeof(STRING)));

    iter = rbh_iter_ring(ring, 1);
    ck_assert_ptr_nonnull(iter);

    for (size_t i = 0; i < sizeof(STRING); i++)
        ck_assert_mem_eq(rbh_iter_next(iter), &STRING[i], 1);

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(iter));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(iter);

    rbh_ring_peek(ring, &readable);
    ck_assert_uint_eq(readable, 0);

    rbh_ring_destroy(ring);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              rbh_iter_list()                               |
 *----------------------------------------------------------------------------*/

START_TEST(ril_empty)
{
    struct rbh_iterator *iter;
    struct rbh_list_node list;

    rbh_list_init(&list);
    iter = rbh_iter_list(&list, 0, NULL);
    ck_assert_ptr_nonnull(iter);

    ck_assert_int_eq(rbh_list_empty(&list), 1);
    ck_assert_ptr_null(rbh_iter_next(iter));
    ck_assert_int_eq(errno, ENODATA);
}
END_TEST

struct list_elem {
        int value;
        struct rbh_list_node link;
};

START_TEST(ril_basic)
{
    struct list_elem values[4] = {
        { .value = 1 },
        { .value = 2 },
        { .value = 3 },
        { .value = 4 },
    };
    struct rbh_iterator *iter;
    struct rbh_list_node list;

    rbh_list_init(&list);

    for (int i = 0; i < 4; i++)
        rbh_list_add_tail(&list, &values[i].link);

    iter = rbh_iter_list(&list, offsetof(struct list_elem, link), NULL);
    ck_assert_ptr_nonnull(iter);

    for (int i = 0; i < 4; i++) {
        const struct list_elem *node;

        node = rbh_iter_next(iter);
        ck_assert_ptr_nonnull(node);
        ck_assert_int_eq(node->value, values[i].value);
    }

    ck_assert_ptr_null(rbh_iter_next(iter));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(iter);
}
END_TEST

static bool ril_freed = false;

static void
free_nodes(struct rbh_list_node *list)
{
    struct list_elem *elem, *tmp;

    rbh_list_foreach_safe(list, elem, tmp, link) {
        rbh_list_del(&elem->link);
        free(elem);
    }

    ril_freed = true;
}

START_TEST(ril_free)
{
    struct rbh_iterator *iter;
    struct rbh_list_node list;

    rbh_list_init(&list);

    for (int i = 0; i < 4; i++) {
        struct list_elem *node = malloc(sizeof(*node));
        ck_assert_ptr_nonnull(node);
        node->value = i;
        rbh_list_add_tail(&list, &node->link);
    }

    iter = rbh_iter_list(&list, offsetof(struct list_elem, link), free_nodes);
    ck_assert_ptr_nonnull(iter);

    for (int i = 0; i < 4; i++) {
        const struct list_elem *node;

        node = rbh_iter_next(iter);
        ck_assert_ptr_nonnull(node);
        ck_assert_int_eq(node->value, i);
    }

    ck_assert_ptr_null(rbh_iter_next(iter));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(iter);
    ck_assert_int_eq(ril_freed, 1);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("itertools");
    tests = tcase_create("rbh_iter_array()");
    tcase_add_test(tests, ria_basic);
    tcase_add_test(tests, ria_free);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_iter_chunkify()");
    tcase_add_test(tests, richu_basic);
    tcase_add_test(tests, richu_with_null_elements);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_iter_tee()");
    tcase_add_test(tests, rit_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_iter_chain()");
    tcase_add_test(tests, richa_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_iter_constify()");
    tcase_add_test(tests, rico_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_iter_ring()");
    tcase_add_test(tests, rir_basic);

    tests = tcase_create("rbh_iter_list()");
    tcase_add_test(tests, ril_empty);
    tcase_add_test(tests, ril_basic);
    tcase_add_test(tests, ril_free);

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
