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

START_TEST(dedup_no_dedup)
{
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *parent;
    struct rbh_id *id;

    id = fake_id();
    parent = fake_id();
    fake_create(&fake_events[0], id, parent);
    fake_upsert(&fake_events[1], id, RBH_STATX_ATIME, NULL);

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = deduplicator_new(20, fake_source);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);
}
END_TEST

START_TEST(dedup_link_unlink)
{
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_id *fake_parent;
    struct rbh_id *id;

    id = fake_id();
    fake_parent = fake_id();

    fake_link(&fake_events[0], id, "test", fake_parent);
    fake_unlink(&fake_events[1], id, "test", fake_parent);

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = deduplicator_new(20, fake_source);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_null(events);

    free(id);
    free(fake_parent);
}
END_TEST

START_TEST(dedup_link_unlink_same_entry_different_parents)
{
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_id *fake_parents[2];
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();
    fake_parents[0] = fake_id();
    fake_parents[1] = fake_id();

    fake_link(&fake_events[0], id, "test", fake_parents[0]);
    fake_unlink(&fake_events[1], id, "test", fake_parents[1]);

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = deduplicator_new(20, fake_source);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_link(event, id, "test", fake_parents[0]);

    event = rbh_mut_iter_next(events);
    ck_assert_unlink(event, id, "test", fake_parents[1]);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
}
END_TEST

START_TEST(dedup_create_delete)
{
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[4];
    struct rbh_mut_iterator *events;
    struct rbh_id *fake_parent;
    struct rbh_id *id;

    id = fake_id();
    fake_parent = fake_id();

    fake_link(&fake_events[0], id, "test", fake_parent);
    fake_link(&fake_events[1], id, "test1", fake_parent);
    fake_unlink(&fake_events[2], id, "test1", fake_parent);
    fake_delete(&fake_events[3], id);

    fake_source = event_list_source(fake_events, 4);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = deduplicator_new(20, fake_source);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_null(events);

    free(id);
    free(fake_parent);
}
END_TEST

START_TEST(dedup_last_unlink)
{
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[4];
    struct rbh_mut_iterator *events;
    struct rbh_id *fake_parent;
    struct rbh_id *id;

    id = fake_id();
    fake_parent = fake_id();

    fake_unlink(&fake_events[0], id, "test", fake_parent);
    fake_unlink(&fake_events[1], id, "test1", fake_parent);
    fake_delete(&fake_events[2], id);

    fake_source = event_list_source(fake_events, 3);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = deduplicator_new(20, fake_source);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    free(id);
    free(fake_parent);
}
END_TEST

START_TEST(dedup_upsert_no_statx)
{
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();

    fake_upsert(&fake_events[0], id, RBH_STATX_ATIME, NULL);
    fake_upsert(&fake_events[1], id, RBH_STATX_MTIME, NULL);

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = deduplicator_new(20, fake_source);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->xattrs.pairs[0].value->map.pairs[0].value->uint32,
                     RBH_STATX_ATIME | RBH_STATX_MTIME
                     );

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
}
END_TEST

START_TEST(dedup_upsert_statx)
{
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[4];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_statx stats[3];
    struct rbh_id *id;

    memset(stats, 0, sizeof(stats));
    id = fake_id();

    stats[0].stx_mask = RBH_STATX_ATIME;
    stats[0].stx_atime.tv_sec = 1234;
    stats[1].stx_mask = RBH_STATX_MTIME;
    stats[1].stx_mtime.tv_sec = 4321;
    stats[2].stx_mask = RBH_STATX_CTIME;
    stats[2].stx_ctime.tv_sec = 2143;

    fake_upsert(&fake_events[0], id, RBH_STATX_MODE, NULL);
    fake_upsert(&fake_events[1], id, RBH_STATX_GID, &stats[0]);
    fake_upsert(&fake_events[2], id, RBH_STATX_UID, &stats[1]);
    fake_upsert(&fake_events[3], id, RBH_STATX_DEV, &stats[2]);

    fake_source = event_list_source(fake_events, 4);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = deduplicator_new(20, fake_source);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->type, RBH_FET_UPSERT);
    ck_assert_ptr_nonnull(event->upsert.statx);
    ck_assert_int_eq(event->upsert.statx->stx_mask,
                     RBH_STATX_ATIME | RBH_STATX_MTIME | RBH_STATX_CTIME
                     );
    ck_assert_int_eq(event->xattrs.pairs[0].value->map.pairs[0].value->uint32,
                     RBH_STATX_MODE | RBH_STATX_GID | RBH_STATX_UID |
                     RBH_STATX_DEV
                     );
    ck_assert_int_eq(event->upsert.statx->stx_atime.tv_sec, 1234);
    ck_assert_int_eq(event->upsert.statx->stx_mtime.tv_sec, 4321);
    ck_assert_int_eq(event->upsert.statx->stx_ctime.tv_sec, 2143);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
}
END_TEST

START_TEST(dedup_upsert_statx_symlink)
{
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_statx stat;
    struct rbh_id *id;

    memset(&stat, 0, sizeof(stat));
    id = fake_id();

    stat.stx_mask = RBH_STATX_ATIME;
    stat.stx_atime.tv_sec = 1234;

    fake_upsert(&fake_events[0], id, RBH_STATX_MODE, &stat);
    fake_symlink(&fake_events[1], id);

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = deduplicator_new(20, fake_source);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->type, RBH_FET_UPSERT);
    ck_assert_ptr_nonnull(event->upsert.statx);
    ck_assert_str_eq(event->xattrs.pairs[0].key, "rbh-fsevents");
    ck_assert_int_eq(event->xattrs.pairs[0].value->type, RBH_VT_MAP);
    ck_assert_int_eq(event->xattrs.pairs[0].value->map.count, 2);

    ck_assert_int_eq(event->upsert.statx->stx_mask, RBH_STATX_ATIME);
    ck_assert_int_eq(event->xattrs.pairs[0].value->map.pairs[0].value->uint32,
                     RBH_STATX_MODE);
    ck_assert_int_eq(event->upsert.statx->stx_atime.tv_sec, 1234);

    ck_assert_str_eq(event->xattrs.pairs[0].value->map.pairs[1].value->string,
                     "symlink");

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
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
    tcase_add_test(tests, dedup_no_dedup);
    tcase_add_test(tests, dedup_link_unlink);
    tcase_add_test(tests, dedup_link_unlink_same_entry_different_parents);
    tcase_add_test(tests, dedup_create_delete);
    tcase_add_test(tests, dedup_last_unlink);
    tcase_add_test(tests, dedup_upsert_no_statx);
    tcase_add_test(tests, dedup_upsert_statx);
    tcase_add_test(tests, dedup_upsert_statx_symlink);

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
