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
#include <stdlib.h>
#include <string.h>

#include "robinhood/stack.h"
#include "robinhood/utils.h"

struct rbh_stack {
    size_t size;
    size_t used;
    char *buffer;
};

struct rbh_stack *
rbh_stack_new(size_t size)
{
    struct rbh_stack *stack;
    char *buffer;

    buffer = xmalloc(size);
    stack = xmalloc(sizeof(*stack));

    stack->buffer = buffer;
    stack->used = 0;
    stack->size = size;

    return stack;
}

static char *
rbh_stack_top(struct rbh_stack *stack)
{
    return stack->buffer + (stack->size - stack->used);
}

void *
rbh_stack_push(struct rbh_stack *stack, const void *data, size_t size)
{
    if (size > stack->size - stack->used) {
        errno = size > stack->size ? EINVAL : ENOBUFS;
        return NULL;
    }

    stack->used += size;
    if (data != NULL)
        memcpy(rbh_stack_top(stack), data, size);
    return rbh_stack_top(stack);
}

void *
rbh_stack_peek(struct rbh_stack *stack, size_t *readable)
{
    *readable = stack->used;
    return rbh_stack_top(stack);
}

int
rbh_stack_pop(struct rbh_stack *stack, size_t count)
{
    if (count > stack->used) {
        errno = EINVAL;
        return -1;
    }

    stack->used -= count;
    return 0;
}

void
rbh_stack_destroy(struct rbh_stack *stack)
{
    free(stack->buffer);
    free(stack);
}
