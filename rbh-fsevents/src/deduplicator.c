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
    struct rbh_list_node *nodedup_queue;
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

struct no_dedup_node {
    struct rbh_fsevent *fsevent;
    struct rbh_list_node link;
};

static void *
no_dedup_iter_next(void *iterator)
{
    struct deduplicator *deduplicator = iterator;
    struct rbh_fsevent *fsevent = NULL;
    struct no_dedup_node *node;
    bool in_process;
    int rc;

    // Check if an fsevent can be dequeued
    struct no_dedup_node *elem, *tmp;
    rbh_list_foreach_safe(deduplicator->nodedup_queue, elem, tmp, link) {
        pthread_mutex_lock(deduplicator->pool_mutex);
        in_process = rbh_hashmap_contains(deduplicator->pool_in_process,
                                          &elem->fsevent->id);
        if (in_process) {
            pthread_mutex_unlock(deduplicator->pool_mutex);
            continue;
        }

        fsevent = elem->fsevent;
        rbh_list_del(&elem->link);
        free(elem);
        pthread_mutex_unlock(deduplicator->pool_mutex);
        break;
    }

    // No fsevent has been dequeued, take a new one
    if (fsevent == NULL) {
next:
        fsevent = (struct rbh_fsevent *)
                   rbh_iter_next(&deduplicator->source->fsevents);
        if (fsevent == NULL) {
            if (!rbh_list_empty(deduplicator->nodedup_queue))
                errno = EAGAIN;
            else
                errno = ENODATA;
            return NULL;
        }

        fsevent = rbh_fsevent_clone(fsevent);
        if (fsevent == NULL)
            return NULL;
    }

    while (*deduplicator->avail_batches == 0)
        continue;

    pthread_mutex_lock(deduplicator->pool_mutex);
    in_process = rbh_hashmap_contains(deduplicator->pool_in_process,
                                      &fsevent->id);
    // If already in enrichment, store it to flush it later
    if (in_process) {
        node = malloc(sizeof(*node));
        if (node == NULL) {
            free(fsevent);
            pthread_mutex_unlock(deduplicator->pool_mutex);
            return NULL;
        }

        node->fsevent = fsevent;
        rbh_list_add_tail(deduplicator->nodedup_queue, &node->link);
        pthread_mutex_unlock(deduplicator->pool_mutex);
        goto next;
    }

    rc = rbh_hashmap_set(deduplicator->pool_in_process, &fsevent->id, NULL);
    pthread_mutex_unlock(deduplicator->pool_mutex);
    if (rc)
        return NULL;

    return rbh_iter_array(fsevent, sizeof(struct rbh_fsevent), 1, free);
}

static void
no_dedup_iter_destroy(void *iterator)
{
    struct deduplicator *deduplicator = iterator;

    free(deduplicator->nodedup_queue);
    free(deduplicator);
}

static const struct rbh_mut_iterator_operations NO_DEDUP_ITER_OPS = {
    .next = no_dedup_iter_next,
    .destroy = no_dedup_iter_destroy,
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
        deduplicator->nodedup_queue = malloc(sizeof(struct rbh_list_node));
        if (deduplicator->nodedup_queue == NULL) {
            free(deduplicator);
            return NULL;
        }
        rbh_list_init(deduplicator->nodedup_queue);
    } else {
        deduplicator->batches = DEDUPLICATOR_ITERATOR;
        deduplicator->pool = rbh_fsevent_pool_new(batch_size, source,
                                                  pool_in_process, pool_mutex);
    }

    return &deduplicator->batches;
}
