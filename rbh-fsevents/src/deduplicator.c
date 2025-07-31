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

#include <robinhood/itertools.h>
#include <robinhood/ring.h>

#include "deduplicator.h"
#include "deduplicator/fsevent_pool.h"
#include <lustre/lustreapi.h>

struct deduplicator {
    struct rbh_mut_iterator batches;
    struct rbh_fsevent_pool *pool;
    struct source *source;
};

/*----------------------------------------------------------------------------*
 |                                deduplicator                                |
 *----------------------------------------------------------------------------*/

static void *
deduplicator_iter_next(void *iterator)
{
    static const struct rbh_fsevent *last_fsevent = NULL;
    struct deduplicator *deduplicator = iterator;
    const struct rbh_fsevent *fsevent;
    int rc = 0;

    do {
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
    const struct rbh_fsevent *fsevent;

    fsevent = rbh_iter_next(&deduplicator->source->fsevents);
    if (fsevent == NULL)
        return NULL;

    return rbh_iter_array(fsevent, sizeof(struct rbh_fsevent), 1);
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
deduplicator_new(size_t batch_size, struct source *source)
{
    struct deduplicator *deduplicator;

    deduplicator = malloc(sizeof(*deduplicator));
    if (deduplicator == NULL)
        return NULL;

    deduplicator->source = source;
    if (batch_size == 0) {
        deduplicator->batches = NO_DEDUP_ITERATOR;
    } else {
        deduplicator->batches = DEDUPLICATOR_ITERATOR;
        deduplicator->pool = rbh_fsevent_pool_new(batch_size, source);
    }

    return &deduplicator->batches;
}
