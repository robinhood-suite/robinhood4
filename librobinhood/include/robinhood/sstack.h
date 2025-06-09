/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_SSTACK_H
#define ROBINHOOD_SSTACK_H

/**
 * @file
 *
 * Dynamically growing contiguous by parts stack interface (LIFO)
 *
 * To create an sstack, use rbh_sstack_new().
 *
 * Example: Create an sstack that can contain chunks of up to 32 bytes
 *
 *     sstack = rbh_sstack_new(32);
 *
 *     0                              31
 *     --------------------------------
 *     |            sstack            |
 *     --------------------------------
 *
 * To push data onto an sstack, use rbh_sstack_push(). Each individual chunk of
 * data is guaranteed to be stored contiguously itself, but not necessarily next
 * to a previous chunk.
 *
 * Example: Push 12 bytes onto an sstack, thrice
 *
 *     address = rbh_sstack_push(sstack, "abcdefghijkl", 12);
 *
 *     0                              31
 *     --------------------------------
 *     |                   abcdefghijkl
 *     --------------------------------
 *                         ^
 *                         address
 *
 *     address = rbh_sstack_push(sstack, "mnopqrstuvwx", 12);
 *
 *     0                              31
 *     --------------------------------
 *     |       mnopqrstuvwxabcdefghijkl
 *     --------------------------------
 *             ^
 *             address
 *
 *     address = rbh_sstack_push(sstack, "yz0123456789", 12);
 *
 *     0                              31
 *     --------------------------------
 *     |       mnopqrstuvwxabcdefghijkl
 *     --------------------------------
 *     |                   yz0123456789
 *     --------------------------------
 *                         ^
 *                         address
 *
 * To access data in an sstack, use the returned address from rbh_sstack_push()
 * or use rbh_sstack_peek(). The returned number of bytes is not the total
 * amount of data the sstack contains but rather the length of topmost
 * contiguous chunk.
 *
 * Example: Peek at an sstack
 *
 *     address = rbh_sstack_peek(sstack, &readable_bytes);
 *
 *     0                              31
 *     --------------------------------
 *     |       mnopqrstuvwxabcdefghijkl
 *     --------------------------------
 *     |                   yz0123456789
 *     --------------------------------
 *                         ^
 *                         address, readable_bytes = 12
 *
 * To remove data from an sstack, use rbh_sstack_pop(). One cannot remove more
 * data at once than rbh_sstack_peek() reports.
 *
 * Example: Pop 8 bytes from an sstack
 *
 *     rbh_sstack_pop(sstack, 8);
 *
 *     0                              31
 *     --------------------------------
 *     |       mnopqrstuvwxabcdefghijkl
 *     --------------------------------
 *     |                           6789
 *     --------------------------------
 *
 * To reserve space in an sstack and write to it later, use rbh_sstack_push()
 * with `data' = NULL.
 *
 * Example: Reserve 13 bytes on an sstack and write over them afterwards
 *
 *     address = rbh_sstack_push(sstack, NULL, 13);
 *
 *     0                              31
 *     --------------------------------
 *     |       mnopqrstuvwxabcdefghijkl
 *     --------------------------------
 *     |              ?????????????6789
 *     --------------------------------
 *                    ^
 *                    address
 *
 *     memcpy(address, "ABCDEFGHIJKLM", 13);
 *
 *     0                              31
 *     --------------------------------
 *     |       mnopqrstuvwxabcdefghijkl
 *     --------------------------------
 *     |              ABCDEFGHIJKLM6789
 *     --------------------------------
 */

#include <stddef.h>

struct rbh_sstack;

/**
 * Create a new sstack
 *
 * @param chunk_size    the maximum number of bytes that can be pushed onto the
 *                      sstack at once
 *
 * @return              a pointer to a newly allocated sstack on success, NULL
 *                      on error and errno is set appropriately
 *
 * @error ENOMEM        there was not enough memory available
 */
struct rbh_sstack *
rbh_sstack_new(size_t chunk_size);

/**
 * Push data onto an sstack
 *
 * @param sstack    the sstack to push data onto
 * @param data      a pointer to the data to push onto \p sstack
 * @param size      the number of bytes from \p data to push onto \p sstack
 *
 * @return          the address where \p data was pushed on success, NULL on
 *                  error and errno is set appropriately
 *
 * @error EINVAL    \p size is greater than \p sstack's chunk size
 * @error ENOMEM    there is not enough memory available
 *
 * \p data is guaranteed to be stored contiguously in \p sstack.
 *
 * \p data may be NULL, in which case nothing is copied onto \p sstack. \p size
 * bytes are still reserved in \p sstack. They can be written to using the
 * returned pointer. Reading these bytes before writing them yields undefined
 * behaviour.
 */
void *
rbh_sstack_push(struct rbh_sstack *sstack, const void *data, size_t size);

/**
 * Allocate \p size bytes on the stack respecting the memory alignment constraints
 * of the system.
 *
 * @param sstack    the sstack to push data onto
 * @param size      the number of bytes to push onto \p sstack
 *
 * @return          the address of first allocated byte , NULL on
 *                  error and errno is set appropriately
 *
 * @error EINVAL    \p size is greater than \p sstack's chunk size
 * @error ENOMEM    there is not enough memory available
 */
void *
rbh_sstack_alloc(struct rbh_sstack *sstack, size_t size);

/**
 * Use rbh_sstack_alloc to allocate memory on \p sstack and copy \p str to it
 *
 * @param sstack    the sstack to push data onto
 * @param str       the string to copy on the stack
 *
 * @return          the address of the duplicated string, NULL on
 *                  error and errno is set appropriately
 *
 * @error EINVAL    \p str is greater than \p sstack's chunk size
 * @error ENOMEM    there is not enough memory available
 */
char *
rbh_sstack_strdup(struct rbh_sstack *sstack, const char *str);

char *
rbh_sstack_strndup(struct rbh_sstack *sstack, const char *str, size_t size);

/**
 * Pop all the data on \p sstack
 */
void
rbh_sstack_pop_all(struct rbh_sstack *sstack);

#define RBH_SSTACK_PUSH(_sstack, _data, _size) \
    ({ \
        void *_result = rbh_sstack_push(_sstack, _data, _size); \
        if (!_result) { \
            fprintf(stderr, \
                    "Error: rbh_sstack_push failed at %s (%d): %s (%d)\n", \
                    __FILE__, __LINE__, strerror(errno), errno); \
            exit(EXIT_FAILURE); \
        } \
        _result; \
    })

/**
 * Peek at data in an sstack
 *
 * @param sstack    the sstack to peek at
 * @param readable  set on return to the number of readable bytes in \p sstack
 *
 * @return          the address of the first readable byte in \p sstack
 */
void *
rbh_sstack_peek(struct rbh_sstack *sstack, size_t *readable);

/**
 * Pop data from an sstack
 *
 * @param sstack    the sstack to pop data from
 * @param count     the number of contiguous bytes to pop from \p sstack
 *
 * @return          0 on success, -1 on error and errno is set appropriately
 *
 * @error EINVAL    there are less than \p count readable bytes in \p sstack
 */
int
rbh_sstack_pop(struct rbh_sstack *sstack, size_t count);

/**
 * Discard unused allocated memory in an sstack
 *
 * @param sstack    the sstack to shrink
 */
void
rbh_sstack_shrink(struct rbh_sstack *sstack);

/**
 * Free resources associated with an sstack
 *
 * @param sstack    the sstack to destroy
 */
void
rbh_sstack_destroy(struct rbh_sstack *sstack);

#endif
