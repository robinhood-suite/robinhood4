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
#include <pthread.h>
#include <sys/stat.h>

#include "check-compat.h"
#include "check_macros.h"
#include "utils.h"

#include "deduplicator.h"
#include "../../include/utils.h"

#include <robinhood/statx.h>

static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static int avail_batches = 1;

static struct rbh_mut_iterator *
_deduplicator_new(size_t batch_size, struct source *source,
                  struct rbh_hashmap **pool_in_process)
{
    size_t (*hash_fn)(const void *);

    if (!strcmp(source->name, "lustre"))
        hash_fn = fsevent_pool_hash_lu_id;
    else
        hash_fn = fsevent_pool_hash_id;

    *pool_in_process = rbh_hashmap_new(fsevent_pool_equals, hash_fn, free,
                                       batch_size > 0 ?
                                         batch_size * 100 / 70 : 1);
    if (*pool_in_process == NULL)
        return NULL;

    return deduplicator_new(batch_size, source, *pool_in_process,
                            &pool_mutex, &avail_batches);
}

START_TEST(dedup_basic)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_mut_iterator *events;

    fake_source = empty_source();
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_null(events);
    ck_assert_int_eq(errno, ENODATA);

    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
}
END_TEST

START_TEST(dedup_one_event)
{
    struct rbh_hashmap *pool_in_process = NULL;
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

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_id_eq(id, &event->id);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
    free(parent);
}
END_TEST

START_TEST(dedup_many_events)
{
    struct rbh_hashmap *pool_in_process = NULL;
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

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
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

    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
    free(parent);
}
END_TEST

START_TEST(dedup_no_dedup)
{
    struct rbh_hashmap *pool_in_process = NULL;
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

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
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

    free(id);
    free(parent);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_link_unlink)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_id *parent;
    struct rbh_id *id;

    id = fake_id();
    parent = fake_id();

    fake_link(&fake_events[0], id, "test", parent);
    fake_unlink(&fake_events[1], id, "test", parent);

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_null(events);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    free(parent);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_link_unlink_same_entry_different_parents)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_id *parents[2];
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();
    parents[0] = fake_id();
    parents[1] = fake_id();

    fake_link(&fake_events[0], id, "test", parents[0]);
    fake_unlink(&fake_events[1], id, "test", parents[1]);

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_link(event, id, "test", parents[0]);

    event = rbh_mut_iter_next(events);
    ck_assert_unlink(event, id, "test", parents[1]);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    free(parents[0]);
    free(parents[1]);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_create_delete)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[4];
    struct rbh_mut_iterator *events;
    struct rbh_id *parent;
    struct rbh_id *id;

    id = fake_id();
    parent = fake_id();

    fake_link(&fake_events[0], id, "test", parent);
    fake_link(&fake_events[1], id, "test1", parent);
    fake_unlink(&fake_events[2], id, "test1", parent);
    fake_delete(&fake_events[3], id);

    fake_source = event_list_source(fake_events, 4);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_null(events);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    free(parent);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_last_unlink)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[4];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *parent;
    struct rbh_id *id;

    id = fake_id();
    parent = fake_id();

    fake_unlink(&fake_events[0], id, "test", parent);
    fake_unlink(&fake_events[1], id, "test1", parent);
    fake_delete(&fake_events[2], id);

    fake_source = event_list_source(fake_events, 3);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->type, RBH_FET_DELETE);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    free(parent);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_upsert_no_statx)
{
    struct rbh_hashmap *pool_in_process = NULL;
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

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
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
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_upsert_statx)
{
    struct rbh_hashmap *pool_in_process = NULL;
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
    stats[1].stx_mask = RBH_STATX_MTIME | RBH_STATX_ATIME;
    stats[1].stx_mtime.tv_sec = 4321;
    stats[1].stx_atime.tv_sec = 5678;
    stats[2].stx_mask = RBH_STATX_CTIME;
    stats[2].stx_ctime.tv_sec = 2143;

    fake_upsert(&fake_events[0], id, RBH_STATX_MODE, NULL);
    fake_upsert(&fake_events[1], id, RBH_STATX_GID, &stats[0]);
    fake_upsert(&fake_events[2], id, RBH_STATX_UID, &stats[1]);
    fake_upsert(&fake_events[3], id, RBH_STATX_DEV, &stats[2]);

    fake_source = event_list_source(fake_events, 4);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
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
    ck_assert_int_eq(event->upsert.statx->stx_atime.tv_sec, 5678);
    ck_assert_int_eq(event->upsert.statx->stx_mtime.tv_sec, 4321);
    ck_assert_int_eq(event->upsert.statx->stx_ctime.tv_sec, 2143);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_upsert_statx_symlink)
{
    struct rbh_hashmap *pool_in_process = NULL;
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

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
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
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_same_xattr)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();

    fake_xattr(&fake_events[0], id, "key");
    fake_xattr(&fake_events[1], id, "key");

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_different_xattrs)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    const struct rbh_value *xattrs;
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();

    fake_xattr(&fake_events[0], id, "key1");
    fake_xattr(&fake_events[1], id, "key2");

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);

    ck_assert_int_eq(event->type, RBH_FET_XATTR);
    ck_assert_int_eq(event->xattrs.count, 1);
    ck_assert_str_eq(event->xattrs.pairs[0].key, "rbh-fsevents");
    ck_assert_int_eq(event->xattrs.pairs[0].value->type, RBH_VT_MAP);
    ck_assert_int_eq(event->xattrs.pairs[0].value->map.count, 1);
    ck_assert_str_eq(event->xattrs.pairs[0].value->map.pairs[0].key, "xattrs");
    ck_assert_int_eq(event->xattrs.pairs[0].value->map.pairs[0].value->type,
                     RBH_VT_SEQUENCE);

    xattrs = event->xattrs.pairs[0].value->map.pairs[0].value;
    ck_assert_int_eq(xattrs->sequence.count, 2);
    ck_assert_int_eq(xattrs->sequence.values[0].type, RBH_VT_STRING);
    ck_assert_str_eq(xattrs->sequence.values[0].string, "key1");
    ck_assert_int_eq(xattrs->sequence.values[1].type, RBH_VT_STRING);
    ck_assert_str_eq(xattrs->sequence.values[1].string, "key2");

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_same_xattr_different_values)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[4];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();

    fake_xattr_key_value(&fake_events[0], id, "key", "value1");
    fake_xattr_key_value(&fake_events[1], id, "key", "value2");
    fake_xattr_key_value(&fake_events[2], id, "key", "value3");
    fake_xattr_key_value(&fake_events[3], id, "key", "value4");

    fake_source = event_list_source(fake_events, 4);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);

    ck_assert_int_eq(event->type, RBH_FET_XATTR);
    ck_assert_int_eq(event->xattrs.count, 1);
    ck_assert_str_eq(event->xattrs.pairs[0].key, "key");
    ck_assert_int_eq(event->xattrs.pairs[0].value->type, RBH_VT_BINARY);
    ck_assert_str_eq(event->xattrs.pairs[0].value->binary.data, "value4");

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_lustre_xattr)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();

    fake_lustre(&fake_events[0], id);
    fake_lustre(&fake_events[1], id);

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_xattr_merge_lustre_with_xattr)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_value const * sequence;
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();

    fake_xattr(&fake_events[0], id, "test");
    fake_lustre(&fake_events[1], id);

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->xattrs.count, 1);
    ck_assert_str_eq(event->xattrs.pairs[0].key, "rbh-fsevents");
    ck_assert_int_eq(event->xattrs.pairs[0].value->type, RBH_VT_MAP);
    ck_assert_int_eq(event->xattrs.pairs[0].value->map.count, 2);
    ck_assert_str_eq(event->xattrs.pairs[0].value->map.pairs[0].key, "xattrs");
    ck_assert_str_eq(event->xattrs.pairs[0].value->map.pairs[1].key, "lustre");

    sequence = event->xattrs.pairs[0].value->map.pairs[0].value;
    ck_assert_int_eq(sequence->type, RBH_VT_SEQUENCE);
    ck_assert_int_eq(sequence->sequence.count, 1);
    ck_assert_int_eq(sequence->sequence.values[0].type, RBH_VT_STRING);
    ck_assert_str_eq(sequence->sequence.values[0].string, "test");

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_xattr_merge_xattrs_with_lustre)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();

    fake_lustre(&fake_events[0], id);
    fake_xattr(&fake_events[1], id, "test");

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->xattrs.count, 1);
    ck_assert_str_eq(event->xattrs.pairs[0].key, "rbh-fsevents");
    ck_assert_int_eq(event->xattrs.pairs[0].value->type, RBH_VT_MAP);
    ck_assert_int_eq(event->xattrs.pairs[0].value->map.count, 2);
    ck_assert_str_eq(event->xattrs.pairs[0].value->map.pairs[0].key, "lustre");
    ck_assert_str_eq(event->xattrs.pairs[0].value->map.pairs[1].key, "xattrs");

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_xattr_merge_fid_with_lustre)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    const struct rbh_value_map *map;
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();

    fake_fid(&fake_events[0], id);
    fake_lustre(&fake_events[1], id);

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->xattrs.count, 2);
    ck_assert_str_eq(event->xattrs.pairs[0].key, "fid");
    ck_assert_str_eq(event->xattrs.pairs[1].key, "rbh-fsevents");
    ck_assert_int_eq(event->xattrs.pairs[1].value->type, RBH_VT_MAP);

    map = &event->xattrs.pairs[1].value->map;
    ck_assert_int_eq(map->count, 1);
    ck_assert_str_eq(map->pairs[0].key, "lustre");
    ck_assert_ptr_eq(map->pairs[0].value, NULL);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_xattr_merge_lustre_with_fid)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();

    fake_lustre(&fake_events[0], id);
    fake_fid(&fake_events[1], id);

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->xattrs.count, 2);
    ck_assert_str_eq(event->xattrs.pairs[0].key, "rbh-fsevents");
    ck_assert_str_eq(event->xattrs.pairs[1].key, "fid");

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_xattr_merge_xattrs_with_fid)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[2];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();

    fake_fid(&fake_events[0], id);
    fake_xattr(&fake_events[1], id, "test");

    fake_source = event_list_source(fake_events, 2);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->xattrs.count, 2);
    ck_assert_str_eq(event->xattrs.pairs[0].key, "fid");
    ck_assert_str_eq(event->xattrs.pairs[1].key, "rbh-fsevents");

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_xattr_merge_xattrs_fid_and_lustre)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    const struct rbh_value_pair *pairs;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[3];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *id;

    id = fake_id();

    fake_fid(&fake_events[0], id);
    fake_xattr(&fake_events[1], id, "test");
    fake_lustre(&fake_events[2], id);

    fake_source = event_list_source(fake_events, 3);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->xattrs.count, 2);
    ck_assert_str_eq(event->xattrs.pairs[0].key, "fid");
    ck_assert_str_eq(event->xattrs.pairs[1].key, "rbh-fsevents");
    ck_assert_int_eq(event->xattrs.pairs[1].value->type, RBH_VT_MAP);
    ck_assert_int_eq(event->xattrs.pairs[1].value->map.count, 2);

    pairs = event->xattrs.pairs[1].value->map.pairs;
    ck_assert_str_eq(pairs[0].key, "xattrs");
    ck_assert_str_eq(pairs[1].key, "lustre");

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    free(id);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
}
END_TEST

START_TEST(dedup_check_flush_order)
{
    struct rbh_hashmap *pool_in_process = NULL;
    struct rbh_mut_iterator *deduplicator;
    struct source *fake_source = NULL;
    struct rbh_fsevent fake_events[6];
    struct rbh_mut_iterator *events;
    struct rbh_fsevent *event;
    struct rbh_id *ids[3];

    fprintf(stderr, "dedup xattr start\n");

    for (size_t i = 0; i < 3; i++)
        ids[i] = fake_id();

    fake_xattr(&fake_events[0], ids[0], "test");
    fake_xattr(&fake_events[1], ids[1], "test");
    fake_xattr(&fake_events[2], ids[2], "test");

    fake_xattr(&fake_events[3], ids[1], "test");
    fake_xattr(&fake_events[4], ids[0], "test");
    fake_xattr(&fake_events[5], ids[2], "test");

    /* In this configuration, we will have 3 different ids.
     * Each id will have 2 associated events.
     * What we expect is 1 event per id after deduplication.
     * The events should be ordered by oldest event reception.
     * This means that although the events where first received in the order 0,
     * 1, 2, we expect to get the order 1, 0, 2 after the deduplication since 1
     * has not received any events for the longest time.
     */

    fake_source = event_list_source(fake_events, 6);
    ck_assert_ptr_nonnull(fake_source);

    deduplicator = _deduplicator_new(20, fake_source, &pool_in_process);
    ck_assert_ptr_nonnull(deduplicator);

    events = rbh_mut_iter_next(deduplicator);
    ck_assert_ptr_nonnull(events);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_id_eq(ids[1], &event->id);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_id_eq(ids[0], &event->id);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_nonnull(event);
    ck_assert_id_eq(ids[2], &event->id);

    event = rbh_mut_iter_next(events);
    ck_assert_ptr_null(event);
    ck_assert_int_eq(errno, ENODATA);

    for (size_t i = 0; i < 3; i++)
        free(ids[i]);
    rbh_mut_iter_destroy(events);
    rbh_mut_iter_destroy(deduplicator);
    rbh_hashmap_destroy(pool_in_process);
    event_list_source_destroy(fake_source);
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
    tcase_add_test(tests, dedup_same_xattr);
    tcase_add_test(tests, dedup_same_xattr_different_values);
    tcase_add_test(tests, dedup_different_xattrs);
    tcase_add_test(tests, dedup_lustre_xattr);
    tcase_add_test(tests, dedup_xattr_merge_lustre_with_xattr);
    tcase_add_test(tests, dedup_xattr_merge_xattrs_with_lustre);
    tcase_add_test(tests, dedup_xattr_merge_fid_with_lustre);
    tcase_add_test(tests, dedup_xattr_merge_lustre_with_fid);
    tcase_add_test(tests, dedup_xattr_merge_xattrs_with_fid);
    tcase_add_test(tests, dedup_xattr_merge_xattrs_fid_and_lustre);
    tcase_add_test(tests, dedup_check_flush_order);

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
