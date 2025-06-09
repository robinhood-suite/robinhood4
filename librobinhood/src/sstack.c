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
#include <string.h>

#include "robinhood/stack.h"
#include "robinhood/sstack.h"

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
    if (stack == NULL)
        return NULL;

    sstack = malloc(sizeof(*sstack));
    if (sstack == NULL) {
        int save_errno = errno;

        rbh_stack_destroy(stack);
        errno = save_errno;
        return NULL;
    }

    sstack->stacks = malloc(sizeof(*sstack->stacks));
    if (sstack->stacks == NULL) {
        int save_errno = errno;

        free(sstack);
        rbh_stack_destroy(stack);
        errno = save_errno;
        return NULL;
    }

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

        tmp = reallocarray(tmp, new_count, sizeof(*tmp));
        if (tmp == NULL) {
            sstack->current--;
            return NULL;
        }

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
rbh_sstack_alloc(struct rbh_sstack *sstack, size_t size)
{
    size_t align = _Alignof(max_align_t);

    if (size & (align - 1))
        /* allocate a bit more to make sure data is always properly aligned */
        size = (size & ~(align - 1)) + align;

    return rbh_sstack_push(sstack, NULL, size);
}

char *
rbh_sstack_strdup(struct rbh_sstack *sstack, const char *str)
{
    size_t len = strlen(str) + 1;
    char *dup;

    dup = rbh_sstack_alloc(sstack, len);
    strcpy(dup, str);

    return dup;
}

char *
rbh_sstack_strndup(struct rbh_sstack *sstack, const char *str, size_t size)
{
    char *dup;

    dup = rbh_sstack_alloc(sstack, size + 1);
    strncpy(dup, str, size);
    dup[size] = '\0';

    return dup;
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
rbh_sstack_pop_all(struct rbh_sstack *sstack)
{
    while (sstack->current != 0) {
        struct rbh_stack *stack = sstack->stacks[sstack->current];
        size_t count;

        rbh_stack_peek(stack, &count);
        rbh_stack_pop(stack, count);
        sstack->current--;
    }
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
