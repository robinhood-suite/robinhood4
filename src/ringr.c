/* This file is part of the RobinHood Library
 * Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include "robinhood/ring.h"
#include "ring.h"

struct rbh_ringr {
    struct rbh_ring *ring;
    char *head;
    bool starved; /* to be considered only if head == ring->head */
    struct rbh_ringr *next;
};

struct rbh_ringr *
rbh_ringr_new(size_t size)
{
    struct rbh_ringr *ringr;
    struct rbh_ring *ring;

    ring = rbh_ring_new(size);
    if (ring == NULL)
        return NULL;

    ringr = malloc(sizeof(*ringr));
    if (ringr == NULL) {
        int save_errno = errno;

        rbh_ring_destroy(ring);
        errno = save_errno;
        return NULL;
    }

    ringr->ring = ring;
    ringr->head = ring->head;
    ringr->next = ringr;
    ringr->starved = false;

    return ringr;
}

struct rbh_ringr *
rbh_ringr_dup(struct rbh_ringr *ringr)
{
    struct rbh_ringr *duplicate;

    duplicate = malloc(sizeof(*duplicate));
    if (duplicate == NULL)
        return NULL;

    duplicate->ring = ringr->ring;
    duplicate->head = ringr->head;
    duplicate->starved = ringr->starved;
    duplicate->next = ringr->next;
    ringr->next = duplicate;

    return duplicate;
}

void *
rbh_ringr_push(struct rbh_ringr *ringr, const void *data, size_t size)
{
    struct rbh_ringr *head = ringr;
    void *address;

    address = rbh_ring_push(ringr->ring, data, size);
    if (address == NULL)
        return NULL;

    if (size == 0 || ringr->ring->used != ringr->ring->size)
        return address;

    do {
        ringr->starved = false;
        ringr = ringr->next;
    } while (ringr != head);

    return address;
}

/* Determine the number of readable bytes from a reader PoV */
static size_t
ringr_readable(const struct rbh_ringr *ringr)
{
    struct rbh_ring *ring = ringr->ring;

    if (ringr->head == ring->head && ringr->starved)
        return 0;

    if (ringr->head >= ring->head)
        return ring->used - (ringr->head - ring->head);
    return ring->head - ringr->head - (ring->size - ring->used);
}

void *
rbh_ringr_peek(struct rbh_ringr *ringr, size_t *readable)
{
    *readable = ringr_readable(ringr);
    return ringr->head;
}

/* Determine the maximum number of readable bytes from all readers PoV
 *
 * TODO: use a max heap instead of a circular list for this search
 */
static size_t
ringr_max_readable(const struct rbh_ringr *ringr)
{
    const struct rbh_ringr *first = ringr;
    size_t max = ringr_readable(first);
    size_t stop = ringr->ring->size;

    for (ringr = ringr->next; ringr != first && max != stop;
         ringr = ringr->next) {
        size_t readable = ringr_readable(ringr);

        if (readable > max)
            max = readable;
    }

    return max;
}

/* Pop unreachable data from the ringr ring ie. data which cannot be read
 * by any reader */
static void
ringr_pop_unreachable(const struct rbh_ringr *ringr)
{
    rbh_ring_pop(ringr->ring, ringr->ring->used - ringr_max_readable(ringr));
}

int
rbh_ringr_ack(struct rbh_ringr *ringr, size_t count)
{
    size_t readable = ringr_readable(ringr);
    struct rbh_ring *ring = ringr->ring;
    char *head = ringr->head;

    if (count > readable) {
        errno = EINVAL;
        return -1;
    }

    if (count == 0)
        return 0;

    ringr->head += count;
    if (ringr->head >= ring->data + ring->size)
        ringr->head -= ring->size;

    if (ringr->head == ring->head)
        ringr->starved = true;

    if (ring->head != head)
        return 0;

    ringr_pop_unreachable(ringr);
    return 0;
}

void
rbh_ringr_destroy(struct rbh_ringr *ringr)
{
    if (ringr->next != ringr) {
        struct rbh_ringr *prev = ringr;

        while (prev->next != ringr)
            prev = prev->next;

        prev->next = ringr->next;
        ringr_pop_unreachable(prev);
    } else {
        rbh_ring_destroy(ringr->ring);
    }

    free(ringr);
}
