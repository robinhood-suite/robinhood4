/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_QUEUE_H
#define ROBINHOOD_QUEUE_H

/**
 * @file
 *
 * Dynamically growing contiguous by parts queue interface (FIFO)
 *
 * To create a queue, use rbh_queue_new(). One can only allocate queue whose
 * chunk size is a multiple of the running kernel's page size.
 *
 * Example: Create a queue that can contain chunks of up to 4K bytes (assuming a
 *          4K page size)
 *
 *     queue = rbh_queue_new(1 << 12);
 *
 *     0                              4K
 *     --------------------------------
 *     |            queue             |
 *     --------------------------------
 *
 * To push data into a queue, use rbh_queue_push(). Each individual chunk of
 * data is guaranteed to be stored contiguously itself, but not necessarily next
 * to a previous chunk.
 *
 * Example: Push 1.5K into a queue, thrice
 *
 *     char buffer[1 << 10 + 1 << 9];
 *
 *     memset(buffer, 0, sizeof(buffer));
 *     address = rbh_queue_push(queue, buffer, sizeof(buffer));
 *
 *     0           1.5K               4K
 *     --------------------------------
 *     000000(...)0                   |
 *     --------------------------------
 *     ^
 *     address
 *
 *     memset(buffer, 1, sizeof(buffer));
 *     address = rbh_queue_push(queue, buffer, sizeof(buffer));
 *
 *     0           1.5K        3K     4K
 *     --------------------------------
 *     000000(...)0111111(...)1       |
 *     --------------------------------
 *                 ^
 *                 address
 *
 *     memset(buffer, 2, sizeof(buffer));
 *     address = rbh_queue_push(queue, buffer, sizeof(buffer));
 *
 *     0           1.5K        3K     4K
 *     --------------------------------
 *     000000(...)0111111(...)1       |
 *     --------------------------------
 *     222222(...)2                   |
 *     --------------------------------
 *     ^
 *     address
 *
 * To access data in a queue, use the returned address from rbh_queue_push() or
 * use rbh_queue_peek(). The returned number of bytes is not the total amount of
 * data the queue contains but rather the length of the oldest contiguous chunk.
 *
 * Example Peek at a queue
 *
 *     address = rbh_queue_peek(queue, &readable_bytes);
 *
 *  ----
 *  |  v           1.5K        3K     4K
 *  |  --------------------------------
 *  |  000000(...)0111111(...)1       |
 *  |  --------------------------------
 *  |  222222(...)2                   |
 *  |  --------------------------------
 *  |
 *  -- address, readable_bytes = 3K
 *
 * To remove data from a queue, use rbh_queue_pop(). One cannot remove more data
 * at once than rbh_queue_peek() reports.
 *
 * Example: Pop 2K bytes from a queue
 *
 *     rbh_queue_pop(queue, 1 << 1);
 *
 *     0           1.5K        3K     4K
 *     --------------------------------
 *     |               11(...)1       |
 *     --------------------------------
 *     222222(...)2                   |
 *     --------------------------------
 *
 * To reserve space in a queue and write to it later, use rbh_queue_push() with
 * `data' = NULL.
 *
 * Example: Reserve 1K in queue and write over them afterwards
 *
 *     address = rbh_queue_push(queue, NULL, 1 << 10);
 *
 *     0           1.5K        3K     4K
 *     --------------------------------
 *                     11(...)1       |
 *     --------------------------------
 *     222222(...)2??(...)?           |
 *     --------------------------------
 *                 ^
 *                 address
 *
 *     memset(address, 3, 1 << 10);
 *
 *     0           1.5K        3K     4K
 *     --------------------------------
 *                     11(...)1       |
 *     --------------------------------
 *     222222(...)233(...)3           |
 *     --------------------------------
 *                 ^
 *                 address
 */

#include <stddef.h>

struct rbh_queue;

/**
 * Create a queue
 *
 * @param chunk_size    the maximum number of bytes that can be pushed into the
 *                      queue at once (it must be a multiple of the running
 *                      kernel's page size)
 *
 * @return              a pointer to a newly allocated queue on success, NULL on
 *                      error and errno is set appropriately
 *
 * @error ENOMEM        there was not enough memory available
 *
 * This function may also fail and set errno for any of the errors specified for
 * the routine rbh_ring_new().
 */
struct rbh_queue *
rbh_queue_new(size_t chunk_size);

/**
 * Push an element into a queue
 *
 * @param queue     the queue to use
 * @param data      a pointer to the data to push into \p queue
 * @param size      the size of the data to push into \p queue
 *
 * @return          the address where \p data was copied on success, NULL on
 *                  error and errno is set appropriately
 *
 * @error EINVAL    \p size is greater than \p queue's chunk size
 * @error ENOMEM    there was not enough memory available
 *
 * \p data is guaranteed to be contiguously in \p queue.
 *
 * \p data may be NULL, in which case nothing is copied into \p queue. \p size
 * bytes are still reserved in \p queue. They can be written to using the
 * returned pointer. Reading these bytes before writing them yields undefined
 * behaviour.
 */
void *
rbh_queue_push(struct rbh_queue *queue, const void *data, size_t size);

/**
 * Peek at data in a queue
 *
 * @param queue     the queue to peek at
 * @param readable  set to the number of readable bytes in \p queue on return
 *
 * @return          the address of the first readable byte in \p queue
 */
void *
rbh_queue_peek(struct rbh_queue *queue, size_t *readable);

/**
 * Pop data from a queue
 *
 * @param queue     the queue to pop data from
 * @param count     the number of bytes to pop from \p queue
 *
 * @return          0 on success, -1 on error and errno is set appropriately
 *
 * @error EINVAL    \p count is greater than the number of readable bytes in
 *                  \p queue
 */
int
rbh_queue_pop(struct rbh_queue *queue, size_t count);

/**
 * Discard unused allocated memory in a queue
 *
 * @param queue     the queue to shrink
 */
void
rbh_queue_shrink(struct rbh_queue *queue);

/**
 * Free resources associated with a queue
 *
 * @param queue     the queue to destroy
 */
void
rbh_queue_destroy(struct rbh_queue *queue);

#endif
