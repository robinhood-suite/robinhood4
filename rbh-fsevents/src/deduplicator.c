/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>

#include <robinhood/itertools.h>
#include <robinhood/ring.h>

#include "deduplicator.h"
#include "deduplicator/fsevent_pool.h"

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
    struct deduplicator *deduplicator = iterator;
    const struct rbh_fsevent *fsevent;

    do {
        int rc;

        fsevent = rbh_iter_next(&deduplicator->source->fsevents);
        if (fsevent == NULL) {
            if (errno == ENODATA)
                break;

            return NULL;
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
            /* last insert filled the pool, flush it now */
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

static const struct rbh_mut_iterator DEDUPLICATOR_ITERATOR = {
    .ops = &DEDUPLICATOR_ITER_OPS,
};

struct rbh_mut_iterator *
deduplicator_new(size_t batch_size, size_t flush_size,
                 struct source *source)
{
    struct deduplicator *deduplicator;

    deduplicator = malloc(sizeof(*deduplicator));
    if (deduplicator == NULL)
        return NULL;

    deduplicator->batches = DEDUPLICATOR_ITERATOR;
    deduplicator->source = source;
    deduplicator->pool = rbh_fsevent_pool_new(batch_size, flush_size);

    return &deduplicator->batches;
}
