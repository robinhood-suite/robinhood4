/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_ITERTOOLS_H
#define ROBINHOOD_ITERTOOLS_H

#include "robinhood/iterator.h"
#include "robinhood/ring.h"
#include "robinhood/list.h"

#include <sys/types.h>

/* @file
 * A collection of utilities to manipulate and build iterators
 */

/**
 * Build an iterator from an array
 *
 * @param array         an array
 * @param element_size  the size of the elements in \p array
 * @param element_count the number of elements in \p array
 * @param free_elem     function to free memory associated with each element
 *
 * @return              a pointer to a newly allocated struct rbh_iterator on
 *                      success, NULL otherwise and errno is set appropriately
 *
 * @retval ENOMEM       there was not enough memory to allocate the iterator
 *
 * The returned iterator's next method yields one element of \p array at a time.
 */
struct rbh_iterator *
rbh_iter_array(const void *array, size_t element_size, size_t element_count,
               void (*free_elem)(void *elem));

/**
 * Build a mutable iterator from an array
 *
 * @param array         an array
 * @param element_size  the size of the elements in \p array
 * @param element_count the number of elements in \p array
 * @param free_elem     function to free memory associated with each element
 *
 * @return              a pointer to a newly allocated struct rbh_mut_iterator
 *                      on success, NULL otherwise and errno is set
 *                      appropriately
 *
 * @retval ENOMEM       there was not enough memory to allocate the iterator
 *
 * The returned iterator's next method yields one element of \p array at a time.
 */
struct rbh_mut_iterator *
rbh_mut_iter_array(void *array, size_t element_size, size_t element_count,
                   void (*free_elem)(void *elem));

/**
 * Split an iterator into several smaller iterators
 *
 * @param iterator  the iterator to split
 * @param chunk     the number of elements each subiterator should yield
 *
 * @return          on success, a pointer to a newly allocated mutable iterator
 *                  that yields iterators, that themselves yields \p chunk
 *                  elements from \p iterator before they appear exhausted;
 *                  NULL on error, and errno is set appropriately
 *
 * @error EINVAL    Invalid value for \p chunk (must be > 0)
 * @error ENOMEM    there was not enough memory available
 *
 * The iterators yielded by the returned iterator must be exhausted sequentially
 * in order to yield elements in the same order \p iterator would have.
 * If maintaining elements in order is not a requirement, this function can be
 * used to parallelize access to \p iterator (assuming it supports concurrent
 * yielding).
 */
struct rbh_mut_iterator *
rbh_iter_chunkify(struct rbh_iterator *iterator, size_t chunk);

/**
 * Produce 2 independent iterators from a single one.
 *
 * @param iterator      the source iterator
 * @param iterators     an array of 2 pointers to iterators, on success those
 *                      will point at 2 independent iterators that will yield
 *                      the same elements as \p iterator would.
 *
 * @return              0 on success, -1 on error and errno is set appropriately
 *
 * @error ENOMEM        there was not enough memory available
 *
 * \p iterator should not be used anymore after a successful call to this
 * function.
 *
 * A reference to each element yielded by one of the generated iterators is kept
 * until the other iterator yields it as well. This can incur a significant
 * memory overhead.
 */
int
rbh_iter_tee(struct rbh_iterator *iterator, struct rbh_iterator *iterators[2]);

/**
 * Produce 2 independent mutable iterators from a single one.
 *
 * @param iterator      the source iterator
 * @param iterators     an array of 2 pointers to mutable iterators, on success
 *                      those will point at 2 independent mutable iterators that
 *                      will yield the same elements as \p iterator would.
 *
 * @return              0 on success, -1 on error and errno is set appropriately
 *
 * @error ENOMEM        there was not enough memory available
 *
 * \p iterator should not be used anymore after a successful call to this
 * function.
 *
 * A reference to each element yielded by one of the generated iterators is kept
 * until the other iterator yields it as well. This can incur a significant
 * memory overhead.
 */
int
rbh_mut_iter_tee(struct rbh_mut_iterator *iterator,
                 struct rbh_mut_iterator *iterators[2]);

/**
 * Chain two independents iterators (in order)
 *
 * @param first     the first iterator to chain
 * @param second    the second iterator to chain
 *
 * @return          a pointer to a newly allocated struct rbh_iterator on
 *                  success, NULL otherwise and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 *
 * \p first and \p second should not be used anymore after a successful call to
 * this function.
 */
struct rbh_iterator *
rbh_iter_chain(struct rbh_iterator *restrict first,
               struct rbh_iterator *restrict second);

/**
 * Chain two independents mutable iterators (in order)
 *
 * @param first     the first iterator to chain
 * @param second    the second iterator to chain
 *
 * @return          a pointer to a newly allocated struct rbh_mut_iterator on
 *                  success, NULL otherwise and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 *
 * \p first and \p second should not be used anymore after a successful call to
 * this function.
 */
struct rbh_mut_iterator *
rbh_mut_iter_chain(struct rbh_mut_iterator *restrict first,
                   struct rbh_mut_iterator *restrict second);

/**
 * Turn a mutable iterator into an immutable iterator without leaking memory
 *
 * @param iterator  a mutable iterator
 *
 * @return          a pointer to a newly allocated struct rbh_iterator on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 *
 * Each entry yielded by \p iterator will be freed automatically.
 *
 * If you need an immutable iterator from a mutable iterator whose elements
 * should not be freed, simply cast that iterator into a struct rbh_iterator.
 *
 * \p iterator should not be used anymore after a successful call to this
 * function.
 */
struct rbh_iterator *
rbh_iter_constify(struct rbh_mut_iterator *iterator);

/**
 * Build an iterator from a ring
 *
 * @param ring          a ring
 * @param element_size  the size of the elements in \p ring
 *
 * @return              a pointer to a newly allocated struct rbh_iterator on
 *                      success, NULL on error and errno is set appropriately
 *
 * @error ENOMEM        there was not enough memory available
 *
 * Each element yielded by the returned iterator will be popped from the ring.
 */
struct rbh_iterator *
rbh_iter_ring(struct rbh_ring *ring, size_t element_size);

/**
 * Build a mutable iterator from a ring
 *
 * @param ring          a ring
 * @param element_size  the size of the elements in \p ring
 *
 * @return              a pointer to a newly allocated struct rbh_mut_iterator
 *                      on success, NULL on error and errno is set appropriately
 *
 * @error ENOMEM        there was not enough memory available
 *
 * Each element yielded by the returned iterator will be popped from the ring.
 */
struct rbh_mut_iterator *
rbh_mut_iter_ring(struct rbh_ring *ring, size_t element_size);

/**
 * Build an iterator from a linked list.
 *
 * @param list    head of the linked list
 * @param offset  offset in bytes of the node in the wrapper structure.
 *                The offset is positive. For example:
 *
 *                struct list_item {
 *                    int value;
 *                    struct rbh_list_node link;
 *                };
 *
 *                struct rbh_list_node *list;
 *
 *                struct rbh_iterator *list_iter = rbh_iter_list(
 *                    list, offsetof(struct list_item, link)
 *                    );
 * @param free_node function to free the memory of the list
 *
 * @return        a pointer to a newly allocated struct rbh_iterator on success,
 *                NULL on error and errno is set appropriately
 *
 * @errro ENOMEM  there was not enough memory available
 */
struct rbh_iterator *
rbh_iter_list(struct rbh_list_node *list, off_t offset,
              void (*free_node)(struct rbh_list_node *list));

#endif
