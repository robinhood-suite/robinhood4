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
#include <stdlib.h>

#include "robinhood/stack.h"
#include "robinhood/sstack.h"
#include "robinhood/utils.h"

struct rbh_sstack {
    struct rbh_stack **stacks;
    size_t chunk_size;

    size_t current;
    size_t count;
    size_t initialized;
};

struct rbh_sstack *
rbh_sstack_new(size_t chunk_size)
{
    struct rbh_sstack *sstack;
    struct rbh_stack *stack;

    stack = rbh_stack_new(chunk_size);

    sstack = xmalloc(sizeof(*sstack));
    sstack->stacks = xmalloc(sizeof(*sstack->stacks));

    sstack->stacks[0] = stack;
    sstack->chunk_size = chunk_size;
    sstack->current = 0;
    sstack->count = 1;
    sstack->initialized = 1;

    return sstack;
}

void *
rbh_sstack_push(struct rbh_sstack *sstack, const void *data, size_t size)
{
    void *ret;

retry:
    errno = 0;
    ret = rbh_stack_push(sstack->stacks[sstack->current], data, size);
    if (ret != NULL)
        return ret;

    if (errno != ENOBUFS)
        return NULL;

    if (++sstack->current >= sstack->count) {
        struct rbh_stack **tmp = sstack->stacks;
        size_t new_count = sstack->count * 2;

        tmp = xreallocarray(tmp, new_count, sizeof(*tmp));

        sstack->stacks = tmp;
        sstack->count = new_count;
    }

    if (sstack->current < sstack->initialized)
        goto retry;

    assert(sstack->current == sstack->initialized);

    sstack->stacks[sstack->current] = rbh_stack_new(sstack->chunk_size);
    if (sstack->stacks[sstack->current] == NULL) {
        sstack->current--;
        return NULL;
    }
    sstack->initialized++;

    goto retry;
}

void *
rbh_sstack_peek(struct rbh_sstack *sstack, size_t *readable)
{
    return rbh_stack_peek(sstack->stacks[sstack->current], readable);
}

int
rbh_sstack_pop(struct rbh_sstack *sstack, size_t count)
{
    struct rbh_stack *stack = sstack->stacks[sstack->current];

    if (rbh_stack_pop(stack, count))
        return -1;

    rbh_stack_peek(stack, &count);
    if (count == 0 && sstack->current > 0)
        sstack->current--;

    return 0;
}

void
rbh_sstack_shrink(struct rbh_sstack *sstack)
{
    for (size_t i = sstack->current + 1; i < sstack->initialized; i++)
        rbh_stack_destroy(sstack->stacks[i]);
    sstack->initialized = sstack->current + 1;
}

void
rbh_sstack_destroy(struct rbh_sstack *sstack)
{
    for (size_t i = 0; i < sstack->initialized; i++)
        rbh_stack_destroy(sstack->stacks[i]);
    free(sstack->stacks);
    free(sstack);
}
