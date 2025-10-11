/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h> /* for SIZE_MAX */
#include <stdlib.h>
#include <string.h>

#include "robinhood/queue.h"
#include "robinhood/ring.h"
#include "robinhood/sstack.h"
#include "robinhood/utils.h"

struct rbh_queue {
    struct rbh_ring **rings;
    size_t chunk_size;

    struct rbh_sstack *pool;

    size_t head;
    size_t tail;
    size_t count;
};

#define QUEUE_POOL_START_COUNT 128

struct rbh_queue *
rbh_queue_new(size_t chunk_size)
{
    struct rbh_queue *queue;
    struct rbh_ring *ring;

    ring = rbh_ring_new(chunk_size);
    if (ring == NULL)
        return NULL;

    queue = xmalloc(sizeof(*queue));
    queue->rings = xmalloc(sizeof(*queue->rings));
    queue->pool = rbh_sstack_new(QUEUE_POOL_START_COUNT * sizeof(ring));

    queue->rings[0] = ring;
    queue->chunk_size = chunk_size;
    queue->head = queue->tail = 0;
    queue->count = 1;

    return queue;
}

void *
rbh_queue_push(struct rbh_queue *queue, const void *data, size_t size)
{
    struct rbh_ring **ring_ptr;
    size_t readable;
    void *ret;

retry:
    ret = rbh_ring_push(queue->rings[queue->tail], data, size);
    if (ret != NULL)
        return ret;

    if (errno != ENOBUFS)
        return NULL;

    if (++queue->tail >= queue->count) {
        /* Are there many unused slots in queue->rings? */
        if (queue->head > queue->count / 2) { /* Yes */
            memmove(queue->rings, &queue->rings[queue->head],
                    (queue->tail - queue->head) * sizeof(*queue->rings));
            queue->tail -= queue->head;
            queue->head = 0;
        } else { /* No */
            struct rbh_ring **tmp = queue->rings;
            size_t new_count = queue->count * 2;

            tmp = xreallocarray(tmp, new_count, sizeof(*tmp));

            queue->rings = tmp;
            queue->count = new_count;
        }
    }

    /* Are there pre-initialized ring buffers in the pool? */
    ring_ptr = rbh_sstack_peek(queue->pool, &readable);
    if (readable) {
        assert(readable % sizeof(struct rbh_ring *) == 0);
        queue->rings[queue->tail] = *ring_ptr;
        rbh_sstack_pop(queue->pool, sizeof(struct rbh_ring *));
    } else {
        queue->rings[queue->tail] = rbh_ring_new(queue->chunk_size);
        if (queue->rings[queue->tail] == NULL) {
            queue->tail--;
            return NULL;
        }
    }

    goto retry;
}

void *
rbh_queue_peek(struct rbh_queue *queue, size_t *readable)
{
    return rbh_ring_peek(queue->rings[queue->head], readable);
}

int
rbh_queue_pop(struct rbh_queue *queue, size_t count)
{
    struct rbh_ring *ring = queue->rings[queue->head];

    if (rbh_ring_pop(ring, count))
        return -1;

    rbh_ring_peek(ring, &count);
    if (count == 0 && queue->head < queue->tail) {
        /* Try to keep a reference on the ring to re-use it later */
        if (rbh_sstack_push(queue->pool, &ring, sizeof(ring)) == NULL)
            /* Better waste resources than fail here */
            rbh_ring_destroy(ring);
        queue->head++;
    }

    return 0;
}

void
rbh_queue_shrink(struct rbh_queue *queue)
{
    /* Free the preallocated rings in `queue->pool' */
    do {
        struct rbh_ring **rings;
        size_t readable;

        rings = rbh_sstack_peek(queue->pool, &readable);
        if (readable == 0)
            break;
        assert(readable % sizeof(*rings) == 0);

        for (size_t i = 0; i < readable / sizeof(*rings); i++)
            rbh_ring_destroy(rings[i]);

        rbh_sstack_pop(queue->pool, readable);
    } while (true);

    /* And shrink the pool itself */
    return rbh_sstack_shrink(queue->pool);
}

void
rbh_queue_destroy(struct rbh_queue *queue)
{
    rbh_queue_shrink(queue);
    rbh_sstack_destroy(queue->pool);

    /* By definition of the size_t type, queue->tail shoud never reach SIZE_MAX,
     * but just as a reminder, if it ever does, the for loop below would never
     * stop ("(size_t)i <= SIZE_MAX" is always true).
     */
    assert(queue->tail < SIZE_MAX);
    for (size_t i = queue->head; i <= queue->tail; i++)
        rbh_ring_destroy(queue->rings[i]);
    free(queue->rings);
    free(queue);
}
