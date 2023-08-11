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
#include <limits.h>
#include <stdlib.h>

#include <sys/stat.h>

#include "check-compat.h"
#include "check_macros.h"
#include "utils.h"

#include "deduplicator.h"

#include <robinhood/statx.h>

START_TEST(dedup_basic)
{
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_mut_iterator *events;

    fake_source = empty_source();
    ck_assert_ptr_nonnull(fake_source);

    // FIXME what value should 10 get?
    deduplicator = deduplicator_new(10, fake_source);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_null(events);
    ck_assert_int_eq(errno, ENODATA);
}
END_TEST

START_TEST(dedup_one_event)
{
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_mut_iterator *events;
    struct rbh_fsevent fake_event;
    struct rbh_fsevent *event;
    struct rbh_id *parent;
    struct rbh_id *id;

    id = fake_id();
    parent = fake_id();
    fake_create(&fake_event, id, parent);

    fake_source = event_list_source(&fake_event, 1);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = deduplicator_new(10, fake_source);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_id_eq(id, &event->id);

    free(id);
}
END_TEST

START_TEST(dedup_many_events)
{
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[5];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *parent;
    struct rbh_id *ids[5];

    parent = fake_id();
    for (int i = 0; i < 5; i++) {
        ids[i] = fake_id();
        fake_create(&fake_events[i], ids[i], parent);
    }

    fake_source = event_list_source(fake_events, 5);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = deduplicator_new(20, fake_source);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    errno = 0;
    for (int i = 0; i < 5; i++) {
        event = rbh_mut_iter_next(events);
        ck_assert_ptr_nonnull(event);
        ck_assert_int_eq(errno, 0);
        ck_assert_id_eq(ids[i], &event->id);
        free(ids[i]);
    }

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("deduplication iterator");

    tests = tcase_create("deduplicator");
    tcase_add_test(tests, dedup_basic);
    tcase_add_test(tests, dedup_one_event);
    tcase_add_test(tests, dedup_many_events);

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
