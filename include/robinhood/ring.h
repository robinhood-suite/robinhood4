/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_RING_H
#define ROBINHOOD_RING_H

/**
 * @file
 *
 * Ring buffer interface (FIFO)
 *
 * To create a ring buffer, use rbh_ring_new(). One can only allocate ring
 * buffers whose size is a multiple of the running kernel's page size.
 *
 * Example: Create a ring buffer of 4K bytes (assuming a 4K page size)
 *
 *     ring = rbh_ring_new(1 << 12);
 *
 *     0                              4K
 *     --------------------------------
 *     |             ring             |
 *     --------------------------------
 *
 * To push data into a ring, use rbh_ring_push().
 *
 * Example: Push 8 bytes into a ring buffer, twice
 *
 *     address = rbh_ring_push(ring, "abcdefgh", 8);
 *
 *     0                              4K
 *     --------------------------------
 *     abcdefgh                       |
 *     --------------------------------
 *     ^
 *     address
 *
 *     address = rbh_ring_push(ring, "ijklmnop", 8);
 *
 *     0                              4K
 *     --------------------------------
 *     abcdefghijklmnop               |
 *     --------------------------------
 *             ^
 *             address
 *
 * To access data in a ring, use the returned address from rbh_ring_push() or
 * use rbh_ring_peek().
 *
 * Example: Peek at a ring
 *
 *     address = rbh_ring_peek(ring, &readable_bytes)
 *
 *     0                              4K
 *     --------------------------------
 *     abcdefghijklmnop               |
 *     --------------------------------
 *     ^
 *     address, readable_bytes = 16
 *
 * To remove data from a ring, use rbh_ring_pop().
 *
 * Example: Pop 12 bytes from a ring
 *
 *     rbh_ring_pop(ring, 12);
 *
 *     0                              4K
 *     --------------------------------
 *     |           mnop               |
 *     --------------------------------
 *
 * To reserve space in a ring and write to it later, use rbh_ring_push() with
 * `data' = NULL.
 *
 * Example: Reserve 8 bytes in a ring and write over them aftewards
 *
 *     address = rbh_ring_push(ring, NULL, 8);
 *
 *     0                              4K
 *     --------------------------------
 *     |           mnop????????       |
 *     --------------------------------
 *                     ^
 *                     address
 *
 *     memcpy(address, "qrstuvwx", 8);
 *
 *     0                              4K
 *     --------------------------------
 *     |           mnopqrstuvwx       |
 *     --------------------------------
 *
 * Every push is guaranteed to store data contiguously so that the whole ring
 * can always be read in one pass.
 *
 * Example: Reserve (4K - 12) bytes in a ring and write over them afterwards
 *
 *    address = rbh_ring_push(ring, NULL, (1 << 12) - 8);
 *
 *    0                              4K
 *    --------------------------------
 *    |           mnopqrstuvwx??(...)?????????????
 *    --------------------------------
 *                                    ^^^^^^^^^^^^
 *                                    this is an arbitrary representation,
 *                                    not a buffer overflow!
 *
 *    memset(address, 0, (1 << 12) - 8);
 *
 *    0                              4K
 *    --------------------------------
 *    |           mnopqrstuvwx00(...)0000000000000
 *    --------------------------------
 */

#include <stddef.h>

struct rbh_ring;

/**
 * Create a ring buffer
 *
 * @param size      the size of the ring buffer (must be a multiple of the
 *                  running kernel's page size)
 *
 * @return          a pointer to a newly allocated ring buffer on success, NULL
 *                  on error and errno is set appropriately
 *
 * @error EINVAL    \p size is not a multiple of the running kernel's page size
 * @error ENOMEM    there was not enough memory available
 *
 * This function may also fail and set errno for any of the errors specified for
 * the routine ftruncate(2), mmap(2) or close(2).
 */
struct rbh_ring *
rbh_ring_new(size_t size);

/**
 * Push data into a ring buffer
 *
 * @param ring      the ring buffer to push data into
 * @param data      a pointer to the data to push into \p ring
 * @param size      the size of the data to push into \p ring
 *
 * @return          the address in \p ring where \p data was pushed on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOBUFS   there is not enough space in \p ring
 * @error EINVAL    \p size is greater than the total space of \p ring
 *
 * \p data may be NULL, in which case nothing is copied into \p ring. \p size
 * bytes are still reserved in \p ring. They can be written to using the
 * returned pointer. Reading these bytes before writing them yields undefined
 * behaviour.
 */
void *
rbh_ring_push(struct rbh_ring *ring, const void *data, size_t size);

/**
 * Peek at data in a ring buffer
 *
 * @param ring      the ring buffer to peek at
 * @param readable  set to the number of readable bytes in \p ring on return
 *
 * @return          the address of the first readable byte in \p ring
 */
void *
rbh_ring_peek(struct rbh_ring *ring, size_t *readable);

/**
 * Pop data from a ring buffer
 *
 * @param ring      the ring buffer to pop data from
 * @param count     the number of bytes to pop from \p ring
 *
 * @return          0 on success, -1 on error and errno is set appropriately
 *
 * @error EINVAL    \p count is greater than the number of readable bytes in
 *                  \p ring
 */
int
rbh_ring_pop(struct rbh_ring *ring, size_t count);

/**
 * Free resources associated with a ring buffer
 *
 * @param ring  the ring to destroy
 */
void
rbh_ring_destroy(struct rbh_ring *ring);

#endif
