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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "robinhood/itertools.h"
#include "robinhood/queue.h"
#include "robinhood/ring.h"
#include "robinhood/utils.h"

/*----------------------------------------------------------------------------*
 |                              rbh_iter_array()                              |
 *----------------------------------------------------------------------------*/

struct array_iterator {
    struct rbh_iterator iter;

    const char *array;
    size_t size;
    size_t count;

    size_t index;

    void (*free_elem)(void *elem);
};

static const void *
array_iter_next(void *iterator)
{
    struct array_iterator *array = iterator;

    if (array->index < array->count)
        return array->array + (array->size * array->index++);

    errno = ENODATA;
    return NULL;
}

static void
array_iter_destroy(void *iterator)
{
    struct array_iterator *array = iterator;

    if (array->free_elem)
        array->free_elem((void *)array->array);

    free(array);
}

static const struct rbh_iterator_operations ARRAY_ITER_OPS = {
    .next = array_iter_next,
    .destroy = array_iter_destroy,
};

static const struct rbh_iterator ARRAY_ITER = {
    .ops = &ARRAY_ITER_OPS,
};

struct rbh_iterator *
rbh_iter_array(const void *array, size_t element_size, size_t element_count,
               void (*free_elem)(void *elem))
{
    struct array_iterator *iterator;

    iterator = xmalloc(sizeof(*iterator));
    iterator->iter = ARRAY_ITER;

    iterator->array = array;
    iterator->size = element_size;
    iterator->count = element_count;
    iterator->free_elem = free_elem;

    iterator->index = 0;

    return &iterator->iter;
}

/*----------------------------------------------------------------------------*
 |                            rbh_mut_iter_array()                            |
 *----------------------------------------------------------------------------*/

struct rbh_mut_iterator *
rbh_mut_iter_array(void *array, size_t element_size, size_t element_count,
                   void (*free_elem)(void *elem))
{
    return (struct rbh_mut_iterator *)rbh_iter_array(array, element_size,
                                                     element_count, free_elem);
}

/*----------------------------------------------------------------------------*
 |                            rbh_iter_chunkify()                             |
 *----------------------------------------------------------------------------*/

struct chunk_iterator {
    struct rbh_iterator iterator;

    struct rbh_iterator *subiter;
    const void *first;
    size_t count;
    bool once;
};

static const void *
chunk_iter_next(void *iterator)
{
    struct chunk_iterator *chunk = iterator;
    const void *next;
    int save_errno;

    if (!chunk->once) {
        chunk->once = true;
        return chunk->first;
    }

    if (chunk->count == 0) {
        errno = ENODATA;
        return NULL;
    }

    save_errno = errno;
    errno = 0;
    next = rbh_iter_next(chunk->subiter);
    if (next != NULL || errno == 0)
        chunk->count--;

    errno = errno ? : save_errno;
    return next;
}

static void
chunk_iter_destroy(void *iterator)
{
    free(iterator);
}

static const struct rbh_iterator_operations CHUNK_ITER_OPS = {
    .next = chunk_iter_next,
    .destroy = chunk_iter_destroy,
};

static const struct rbh_iterator CHUNK_ITER = {
    .ops = &CHUNK_ITER_OPS,
};

struct chunkify_iterator {
    struct rbh_mut_iterator iterator;

    struct rbh_iterator *subiter;
    size_t chunk;
};

static void *
chunkify_iter_next(void *iterator)
{
    struct chunkify_iterator *chunkify = iterator;
    struct chunk_iterator *chunk;
    const void *first;
    int save_errno;

    save_errno = errno;
    errno = 0;
    first = rbh_iter_next(chunkify->subiter);
    if (first == NULL && errno != 0)
        return NULL;
    errno = save_errno;

    chunk = xmalloc(sizeof(*chunk));

    chunk->iterator = CHUNK_ITER;

    chunk->subiter = chunkify->subiter;
    chunk->first = first;
    chunk->count = chunkify->chunk - 1;
    chunk->once = false;

    return chunk;
}

static void
chunkify_iter_destroy(void *iterator)
{
    struct chunkify_iterator *chunkify = iterator;

    rbh_iter_destroy(chunkify->subiter);
    free(chunkify);
}

static const struct rbh_mut_iterator_operations CHUNKIFY_ITER_OPS = {
    .next = chunkify_iter_next,
    .destroy = chunkify_iter_destroy,
};

static const struct rbh_mut_iterator CHUNKIFY_ITER = {
    .ops = &CHUNKIFY_ITER_OPS,
};

struct rbh_mut_iterator *
rbh_iter_chunkify(struct rbh_iterator *iterator, size_t chunk)
{
    struct chunkify_iterator *chunkify;

    if (chunk == 0) {
        errno = EINVAL;
        return NULL;
    }

    chunkify = xmalloc(sizeof(*chunkify));

    chunkify->iterator = CHUNKIFY_ITER;
    chunkify->subiter = iterator;
    chunkify->chunk = chunk;

    return &chunkify->iterator;
}

/*----------------------------------------------------------------------------*
 |                               rbh_iter_tee()                               |
 *----------------------------------------------------------------------------*/

struct tee_iterator {
    struct rbh_iterator iterator;

    struct rbh_iterator *subiter;
    struct tee_iterator *clone;
    struct rbh_queue *queue;
    const void *next;
};

static int
tee_iter_share(struct tee_iterator *tee, const void *element)
{
    return rbh_queue_push(tee->queue, &element, sizeof(element)) ? 0 : -1;
}

static const void *
_tee_iter_next(struct tee_iterator *tee)
{
    const void *element;

    if (tee->next) {
        element = tee->next;
        tee->next = NULL;
        return element;
    }

    return rbh_iter_next(tee->subiter);
}

static const void *
tee_iter_next(void *iterator)
{
    struct tee_iterator *tee = iterator;
    const void **queue_pointer;
    const void *element;
    size_t readable;
    int save_errno = errno;

    queue_pointer = rbh_queue_peek(tee->queue, &readable);
    if (readable) {
        assert(readable % sizeof(element) == 0);
        memcpy(&element, queue_pointer, sizeof(element));
        rbh_queue_pop(tee->queue, sizeof(element));
        return element;
    }

    /* If the last share failed, retry it now */
    if (tee->clone && tee->clone->next) {
        if (tee_iter_share(tee->clone, tee->clone->next))
            /* Cannot go any further without losing elements */
            return NULL;
        tee->clone->next = NULL;
    }

    errno = 0;
    element = _tee_iter_next(tee);
    if (element == NULL && errno != 0)
        return NULL;

    if (tee->clone != NULL) {
        /* Share this element this element for the other iterator */
        if (tee_iter_share(tee->clone, element))
            /* Sharing failed. Keep `element' somewhere the other side of the
             * tee can access. Will retry sharing later (if need be).
             */
            tee->clone->next = element;
    }

    errno = save_errno;
    return element;
}

static void
tee_iter_destroy(void *iterator)
{
    struct tee_iterator *tee = iterator;

    if (tee->clone != NULL)
        tee->clone->clone = NULL;
    else
        rbh_iter_destroy(tee->subiter);
    rbh_queue_destroy(tee->queue);
    free(tee);
}

static const struct rbh_iterator_operations TEE_ITER_OPS = {
    .next = tee_iter_next,
    .destroy = tee_iter_destroy,
};

static const struct rbh_iterator TEE_ITER = {
    .ops = &TEE_ITER_OPS,
};

static long page_size;

static void
get_page_size(void) __attribute__((constructor));

static void
get_page_size(void)
{
    page_size = sysconf(_SC_PAGESIZE);
}

static struct tee_iterator *
tee_iter_new(struct rbh_iterator *subiter)
{
    struct tee_iterator *tee;
    struct rbh_queue *queue;

    queue = rbh_queue_new(page_size);
    if (queue == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    tee = xmalloc(sizeof(*tee));

    tee->iterator = TEE_ITER;
    tee->subiter = subiter;
    tee->clone = NULL;
    tee->queue = queue;
    tee->next = NULL;

    return tee;
}

int
rbh_iter_tee(struct rbh_iterator *iterator, struct rbh_iterator *iterators[2])
{
    struct tee_iterator *tees[2];

    tees[0] = tee_iter_new(iterator);
    if (tees[0] == NULL)
        return -1;

    tees[1] = tee_iter_new(iterator);
    if (tees[1] == NULL) {
        int save_errno = errno;

        rbh_queue_destroy(tees[0]->queue);
        free(tees[0]);

        errno = save_errno;
        return -1;
    }

    /* Link both tee_iterator with each other */
    tees[0]->clone = tees[1];
    tees[1]->clone = tees[0];

    iterators[0] = &tees[0]->iterator;
    iterators[1] = &tees[1]->iterator;
    return 0;
}

/*----------------------------------------------------------------------------*
 |                             rbh_mut_iter_tee()                             |
 *----------------------------------------------------------------------------*/

int
rbh_mut_iter_tee(struct rbh_mut_iterator *iterator,
                 struct rbh_mut_iterator *iterators[2])
{
    return rbh_iter_tee((struct rbh_iterator *)iterator,
                        (struct rbh_iterator **)iterators);
}

/*----------------------------------------------------------------------------*
 |                              rbh_iter_chain()                              |
 *----------------------------------------------------------------------------*/

struct chain_iterator {
    struct rbh_iterator iterator;
    struct rbh_iterator *first;
    struct rbh_iterator *second;
};

static const void *
chain_iter_next(void *iterator)
{
    struct chain_iterator *chain = iterator;
    const void *element;
    int save_errno;

retry:
    if (chain->first == NULL) {
        errno = ENODATA;
        return NULL;
    }

    save_errno = errno;
    errno = 0;
    element = rbh_iter_next(chain->first);
    if (element != NULL || errno == 0) {
        errno = save_errno;
        return element;
    }
    if (errno != ENODATA)
        return NULL;

    rbh_iter_destroy(chain->first);
    chain->first = chain->second;
    chain->second = NULL;
    goto retry;
}

static void
chain_iter_destroy(void *iterator)
{
    struct chain_iterator *chain = iterator;

    if (chain->first)
        rbh_iter_destroy(chain->first);
    if (chain->second)
        rbh_iter_destroy(chain->second);
    free(chain);
}

static const struct rbh_iterator_operations CHAIN_ITER_OPS = {
    .next = chain_iter_next,
    .destroy = chain_iter_destroy,
};

static const struct rbh_iterator CHAIN_ITER = {
    .ops = &CHAIN_ITER_OPS,
};

struct rbh_iterator *
rbh_iter_chain(struct rbh_iterator *first, struct rbh_iterator *second)
{
    struct chain_iterator *chain;

    if (first == NULL)
        return second;
    if (second == NULL)
        return first;

    chain = xmalloc(sizeof(*chain));

    chain->iterator = CHAIN_ITER;
    chain->first = first;
    chain->second = second;
    return &chain->iterator;
}

/*----------------------------------------------------------------------------*
 |                            rbh_mut_iter_chain()                            |
 *----------------------------------------------------------------------------*/

struct rbh_mut_iterator *
rbh_mut_iter_chain(struct rbh_mut_iterator *first,
                   struct rbh_mut_iterator *second)
{
    return (struct rbh_mut_iterator *)rbh_iter_chain(
            (struct rbh_iterator *)first,
            (struct rbh_iterator *)second
            );
}

/*----------------------------------------------------------------------------*
 |                            rbh_iter_constify()                             |
 *----------------------------------------------------------------------------*/

struct constify_iterator {
    struct rbh_iterator iterator;

    struct rbh_mut_iterator *subiter;
    void *element;
};

static const void *
constify_iter_next(void *iterator)
{
    struct constify_iterator *constify = iterator;

    free(constify->element);
    constify->element = rbh_mut_iter_next(constify->subiter);
    return constify->element;
}

static void
constify_iter_destroy(void *iterator)
{
    struct constify_iterator *constify = iterator;

    free(constify->element);
    rbh_mut_iter_destroy(constify->subiter);
    free(constify);
}

static const struct rbh_iterator_operations CONSTIFY_ITER_OPS = {
    .next = constify_iter_next,
    .destroy = constify_iter_destroy,
};

static const struct rbh_iterator CONSTIFY_ITERATOR = {
    .ops = &CONSTIFY_ITER_OPS,
};

struct rbh_iterator *
rbh_iter_constify(struct rbh_mut_iterator *iterator)
{
    struct constify_iterator *constify;

    constify = xmalloc(sizeof(*constify));

    constify->iterator = CONSTIFY_ITERATOR;
    constify->subiter = iterator;
    constify->element = NULL;
    return &constify->iterator;
}

/*----------------------------------------------------------------------------*
 |                              rbh_iter_ring()                               |
 *----------------------------------------------------------------------------*/

struct ring_iterator {
    struct rbh_iterator iterator;

    struct rbh_ring *ring;
    size_t size;
};

static const void *
ring_iter_next(void *iterator)
{
    struct ring_iterator *iter = iterator;
    const void *element;
    size_t readable;
    int rc;

    element = rbh_ring_peek(iter->ring, &readable);
    if (readable == 0) {
        errno = ENODATA;
        return NULL;
    }

    assert(readable % iter->size == 0);
    rc = rbh_ring_pop(iter->ring, iter->size);
    assert(rc == 0);
    return element;
}

static const struct rbh_iterator_operations RING_ITER_OPS = {
    .next = ring_iter_next,
    .destroy = free,
};

static const struct rbh_iterator RING_ITERATOR = {
    .ops = &RING_ITER_OPS,
};

struct rbh_iterator *
rbh_iter_ring(struct rbh_ring *ring, size_t element_size)
{
    struct ring_iterator *iterator;

    iterator = xmalloc(sizeof(*iterator));

    iterator->iterator = RING_ITERATOR;
    iterator->ring = ring;
    iterator->size = element_size;
    return &iterator->iterator;
}

/*----------------------------------------------------------------------------*
 |                            rbh_mut_iter_ring()                             |
 *----------------------------------------------------------------------------*/

struct rbh_mut_iterator *
rbh_mut_iter_ring(struct rbh_ring *ring, size_t element_size)
{
    return (struct rbh_mut_iterator *)rbh_iter_ring(ring, element_size);
}

/*----------------------------------------------------------------------------*
 |                            rbh_iter_list()                                 |
 *----------------------------------------------------------------------------*/

struct list_iterator {
    struct rbh_iterator iterator;

    struct rbh_list_node *head;
    struct rbh_list_node *current;
    void (*free_node)(struct rbh_list_node *list);
    off_t offset;
};

static const void *
list_iter_next(void *iterator)
{
    struct list_iterator *iter = iterator;

    assert(iter->current);
    if (iter->current->next == iter->head) {
        errno = ENODATA;
        return NULL;
    }

    iter->current = iter->current->next;

    return ((char *)iter->current - iter->offset);
}

static void
list_iter_destroy(void *iterator)
{
    struct list_iterator *iter = iterator;
    if (iter->free_node)
        iter->free_node(iter->head);
    free(iter);
}

static const struct rbh_iterator_operations LIST_OPS = {
    .next = list_iter_next,
    .destroy = list_iter_destroy,
};

static const struct rbh_iterator LIST_ITERATOR = {
    .ops = &LIST_OPS,
};

struct rbh_iterator *
rbh_iter_list(struct rbh_list_node *list, off_t offset,
              void (*free_node)(struct rbh_list_node *list))
{
    struct list_iterator *iterator;

    iterator = xmalloc(sizeof(*iterator));

    iterator->iterator = LIST_ITERATOR;
    iterator->head = list;
    iterator->current = list;
    iterator->offset = offset;
    iterator->free_node = free_node;

    return &iterator->iterator;
}
