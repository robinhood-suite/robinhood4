/* This file is part of the RobinHood Library
 * Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_RINGR_H
#define ROBINHOOD_RINGR_H

/**
 * @file
 *
 * Ring buffer with multiple readers
 *
 * To create a ring buffer with multiple readers, use rbh_ringr_new() and
 * rbh_ringr_dup().
 *
 * Example: Create a ring buffer of 4K bytes with 2 readers
 *
 *     reader_0 = rbh_ringr_new(1 << 12);
 *     reader_1 = rbh_ringr_dup(reader_0);
 *
 *     0                              4K
 *     --------------------------------
 *     |             ring             |
 *     --------------------------------
 *     ^
 *     reader_0, reader_1
 *
 * To push data, use rbh_ringr_push().
 *
 * Example: Push 16 bytes into the ring buffer
 *
 *     address = rbh_ringr_push(reader_0, "abcdefghijklmnop", 16);
 *
 *     0                              4K
 *     --------------------------------
 *     abcdefghijklmnop               |
 *     --------------------------------
 *     ^
 *     address, r0, r1
 *
 * To access data in a ring from a reader PoV, use rbh_ringr_peek().
 *
 * Example: Peek at the ring from the first reader PoV
 *
 *     address = rbh_ringr_peek(reader_0, &readable_bytes)
 *
 *     0                              4K
 *     --------------------------------
 *     abcdefghijklmnop               |
 *     --------------------------------
 *     ^
 *     r0, readable_bytes = 16
 *
 * To ack data from a reader PoV, use rbh_ringr_ack().
 *
 * Example: Ack 12 bytes from the first reader PoV
 *
 *     rbh_ringr_ack(reader_0, 12);
 *
 *     0                              4K
 *     --------------------------------
 *     abcdefghijklmnop               |
 *     --------------------------------
 *     ^           ^
 *     r1          r0
 */

#include <stddef.h>

struct rbh_ringr;

/**
 * Create a ring buffer with multiple readers (ringr)
 *
 * A ringr structure is a mechanism to allow multiple readers to be
 * associated to the same ring buffer. Data is considered consumed if all
 * readers have read it.
 *
 * @param size      the size of the ring buffer associated to the ringr
 *
 * @return          a pointer to a newly allocated ringr on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 *
 * This function may also fail and set errno for any of the errors specified
 * for the rbh_ring_new() call.
 */
struct rbh_ringr *
rbh_ringr_new(size_t size);

/**
 * Create a new reader for a ringr
 *
 * The new reader head will be positionned at the head of \p ringr
 *
 * @param ringr     the ringr to duplicate to create the new reader
 *
 * @return          a pointer to a newly allocated ringr on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_ringr *
rbh_ringr_dup(struct rbh_ringr *ringr);

/**
 * Push data into a ringr
 *
 * It mainly consists in a wrapper of rbh_ring_push().
 *
 * @param ringr     the ringr to push data into
 * @param data      a pointer to the data to push into \p ringr
 * @param size      the size of the data to push into \p ringr
 *
 * @return          the address in \p ringr 's ring where \p data was pushed on
 *                  success, NULL on error and errno is set appropriately
 *
 * This function may fail and set errno for any of the errors specified for the
 * rbh_ring_push() call.
 */
void *
rbh_ringr_push(struct rbh_ringr *ringr, const void *data, size_t size);

/**
 * Peek at data in a ringr from a reader point of view
 *
 * @param ringr     the ringr to peek at
 * @param readable  set to the number of readable bytes in \p ringr from the
 *                  reader point of view on return
 *
 * @return          the address of the first readable byte in \p ringr, NULL
 *                  on error and errno is set appropriately
 *
 */
void *
rbh_ringr_peek(struct rbh_ringr *ringr, size_t *readable);

/**
 * Ack data from a ringr
 *
 * This is equivalent to popping data from a ring, but only
 * from a given ringr's PoV
 *
 * @param ringr     the ringr to ack
 * @param count     the number of bytes to ack
 *
 * @return          0 on success, -1 on error and errno is set appropriately
 *
 * @error EINVAL    \p count is greater than the number of readable bytes in
 *                  \p ringr from the reader point of view
 *
 * Data is popped out from the ring as soon as it is ack-ed by every reader
 */
int
rbh_ringr_ack(struct rbh_ringr *ringr, size_t count);

/**
 * Free resources associated with a ringr
 *
 * @param ringr     the ringr to destroy
 *
 * All the data in the ring still reachable by \p ringr is automatically ack-ed
 */
void
rbh_ringr_destroy(struct rbh_ringr *ringr);

#endif
