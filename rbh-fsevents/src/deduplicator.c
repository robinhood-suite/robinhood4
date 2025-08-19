/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#include <robinhood/itertools.h>
#include <robinhood/fsevent.h>
#include <robinhood/ring.h>
#include <robinhood/hashmap.h>

#include "deduplicator.h"
#include "deduplicator/fsevent_pool.h"

struct deduplicator {
    struct rbh_mut_iterator batches;
    struct rbh_fsevent_pool *pool;
    struct source *source;
    struct rbh_hashmap *pool_in_process;
    pthread_mutex_t *pool_mutex;
    int *avail_batches;
};

/*----------------------------------------------------------------------------*
 |                                deduplicator                                |
 *----------------------------------------------------------------------------*/

static const struct rbh_fsevent *last_fsevent = NULL;

static void *
deduplicator_iter_next(void *iterator)
{
    struct deduplicator *deduplicator = iterator;
    const struct rbh_fsevent *fsevent;

    do {
        int rc;

        if (last_fsevent != NULL) {
            fsevent = last_fsevent;
            last_fsevent = NULL;
        } else {
            fsevent = rbh_iter_next(&deduplicator->source->fsevents);
            if (fsevent == NULL) {
                if (errno == ENODATA)
                    break;

                return NULL;
            }
        }

        errno = 0;
        rc = rbh_fsevent_pool_push(deduplicator->pool, fsevent);
        if (rc == POOL_ALREADY_FULL) {
            /* The pool is full. No fsevent was inserted. This should not happen
             * since we have flushed the events the first time the pool was full
             * so we abort.
             */
            abort();
        } else if (rc == POOL_INSERT_FAILED) {
            return NULL;
        } else if (rc == POOL_FULL) {
            errno = 0;
            /* current fsevent cannot be inserted, flush it now */
            last_fsevent = fsevent;
            break;
        }
        assert(rc == POOL_INSERT_OK);

    } while (errno == 0);

    /* The pool will be flushed whether the loop was stopped because
     * rbh_iter_next returned NULL or the pool is full and needs to
     * be flushed. In the first case, it means that not enough events
     * were generated and we could not fill the pool completely.
     */

    while(*deduplicator->avail_batches == 0)
        continue;

    return rbh_fsevent_pool_flush(deduplicator->pool);
}

static void
deduplicator_iter_destroy(void *iterator)
{
    struct deduplicator *deduplicator = iterator;

    rbh_fsevent_pool_destroy(deduplicator->pool);
    free(deduplicator);
}

static const struct rbh_mut_iterator_operations DEDUPLICATOR_ITER_OPS = {
    .next = deduplicator_iter_next,
    .destroy = deduplicator_iter_destroy,
};

static void *
no_dedup_iter_next(void *iterator)
{
    struct deduplicator *deduplicator = iterator;
    const struct rbh_fsevent *fsevent_copy;
    const struct rbh_fsevent *fsevent;
    int rc;

    fsevent = rbh_iter_next(&deduplicator->source->fsevents);
    if (fsevent == NULL)
        return NULL;

    fsevent_copy = rbh_fsevent_clone(fsevent);
    if (fsevent_copy == NULL)
        return NULL;

    while (*deduplicator->avail_batches == 0)
        continue;

    pthread_mutex_lock(deduplicator->pool_mutex);
    rc = rbh_hashmap_set(deduplicator->pool_in_process, &fsevent_copy->id,
                         NULL);
    pthread_mutex_unlock(deduplicator->pool_mutex);
    if (rc)
        return NULL;

    return rbh_iter_array(fsevent_copy, sizeof(struct rbh_fsevent), 1, free);
}

static const struct rbh_mut_iterator_operations NO_DEDUP_ITER_OPS = {
    .next = no_dedup_iter_next,
    .destroy = free,
};

static const struct rbh_mut_iterator DEDUPLICATOR_ITERATOR = {
    .ops = &DEDUPLICATOR_ITER_OPS,
};

static const struct rbh_mut_iterator NO_DEDUP_ITERATOR = {
    .ops = &NO_DEDUP_ITER_OPS,
};

struct rbh_mut_iterator *
deduplicator_new(size_t batch_size, struct source *source,
                 struct rbh_hashmap *pool_in_process,
                 pthread_mutex_t *pool_mutex, int *avail_batches)
{
    struct deduplicator *deduplicator;

    deduplicator = malloc(sizeof(*deduplicator));
    if (deduplicator == NULL)
        return NULL;

    deduplicator->source = source;
    deduplicator->pool_mutex = pool_mutex;
    deduplicator->pool_in_process = pool_in_process;
    deduplicator->avail_batches = avail_batches;

    if (batch_size == 0) {
        deduplicator->batches = NO_DEDUP_ITERATOR;
    } else {
        deduplicator->batches = DEDUPLICATOR_ITERATOR;
        deduplicator->pool = rbh_fsevent_pool_new(batch_size, source,
                                                  pool_in_process, pool_mutex);
    }

    return &deduplicator->batches;
}
