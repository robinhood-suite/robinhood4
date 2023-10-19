/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_STACK_H
#define ROBINHOOD_STACK_H

/**
 * @file
 *
 * Fixed-size contiguous stack interface (LIFO)
 *
 * To create a stack, use rbh_stack_new().
 *
 * Example: Create a stack of 32 bytes
 *
 *     stack = rbh_stack_new(32);
 *
 *     0                              31
 *     --------------------------------
 *     |            stack             |
 *     --------------------------------
 *
 * To push data onto a stack, use rbh_stack_push().
 *
 * Example: Push 4 bytes onto a stack, twice
 *
 *     address = rbh_stack_push(stack, "abcd", 4);
 *
 *     0                              31
 *     --------------------------------
 *     |                           abcd
 *     --------------------------------
 *                                 ^
 *                                 address
 *
 *     address = rbh_stack_push(stack, "efgh", 4);
 *
 *     0                              31
 *     --------------------------------
 *     |                       efghabcd
 *     --------------------------------
 *                             ^
 *                             address
 *
 * To access data in a stack, use the returned address from rbh_stack_push() or
 * use rbh_stack_peek().
 *
 * Example: Peek at a stack
 *
 *     address = rbh_stack_peek(stack, &readable_bytes);
 *
 *     0                              31
 *     --------------------------------
 *     |                       efghabcd
 *     --------------------------------
 *                             ^
 *                             address, readable_bytes = 8
 *
 * To remove data from a stack, use rbh_stack_pop().
 *
 * Example: Pop 6 bytes from a stack
 *
 *     rbh_stack_pop(stack, 6);
 *
 *     0                              31
 *     --------------------------------
 *     |                             cd
 *     --------------------------------
 *
 * To reserve space in a stack and write to it later, use rbh_stack_push() with
 * `data' = NULL.
 *
 * Example: Reserve 10 bytes on a stack and write over them aftewards
 *
 *     address = rbh_stack_push(stack, NULL, 10);
 *
 *     0                              31
 *     --------------------------------
 *     |                   ??????????cd
 *     --------------------------------
 *                         ^
 *                         address
 *
 *     memcpy(address, "ijklmnopqr", 10);
 *
 *     0                              31
 *     --------------------------------
 *     |                   ijklmnopqrcd
 *     --------------------------------
 */

#include <stddef.h>

struct rbh_stack;

/**
 * Create a new stack
 *
 * @param size      the size of the stack to allocate
 *
 * @return          a pointer to a newly allocated stack on success, NULL on
 *                  error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_stack *
rbh_stack_new(size_t size);

/**
 * Push data onto a stack
 *
 * @param stack     the stack to push data onto
 * @param data      a pointer to the data to push onto \p stack
 * @param size      the number of bytes from \p data to push onto \p stack
 *
 * @return          the address where \p data was put onto \p stack on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOBUFS   there is not enough space in \p stack to store \p size bytes
 * @error EINVAL    \p size is greater than the total space of \p stack
 *
 * \p data may be NULL, in which case nothing is copied onto \p stack. \p size
 * bytes are still reserved in \p stack. They can be written to using the
 * returned pointer. Reading these bytes before writing them yields undefined
 * behaviour.
 */
void *
rbh_stack_push(struct rbh_stack *stack, const void *data, size_t size);

/**
 * Peek at a stack
 *
 * @param stack     the stack to peek at
 * @param readable  set on return to the number of readable bytes in \p stack
 *
 * @return          the address of the first readable byte in \p stack
 */
void *
rbh_stack_peek(struct rbh_stack *stack, size_t *readable);

/**
 * Pop data from a stack
 *
 * @param stack     the stack to pop data from
 * @param count     the number of bytes to pop from \p stack
 *
 * @return          0 on success, -1 on error and errno is set appropriately
 *
 * @error EINVAL    there are less then \p count readable bytes in \p stack
 */
int
rbh_stack_pop(struct rbh_stack *stack, size_t count);

/**
 * Release resources associated with a stack
 *
 * @param stack     the stack to destroy
 */
void
rbh_stack_destroy(struct rbh_stack *stack);

#endif
