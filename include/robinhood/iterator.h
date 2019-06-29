/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_ITERATOR_H
#define ROBINHOOD_ITERATOR_H

#include <assert.h>
#include <stddef.h>

/** @file
 * An iterator is a standard interface to traverse a collection of objects
 *
 * This interface distinguishes mutable from immutable iterators which
 * respectively yield mutable and immutable references.
 */

/*----------------------------------------------------------------------------*
 |                             immutable iterator                             |
 *----------------------------------------------------------------------------*/

/**
 * Pointers returned by the `next' method of an immutable iterator are
 * guaranteed to stay valid until the iterator that yielded them is destroyed.
 */

struct rbh_iterator {
    const struct rbh_iterator_operations *ops;
};

struct rbh_iterator_operations {
    const void *(*next)(void *iterator);
    void (*destroy)(void *iterator);
};

/**
 * Yield an immutable reference on the next element of an iterator
 *
 * @param iterator  an iterator
 *
 * @return          a const pointer to the next element in the iterator on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error ENODATA   the iterator is exhausted
 */
static inline const void *
rbh_iter_next(struct rbh_iterator *iterator)
{
    assert(iterator);
    assert(iterator->ops);
    assert(iterator->ops->next);
    return iterator->ops->next(iterator);
}

/**
 * Free resources associated to a struct rbh_iterator
 *
 * @param iterator  a pointer to the struct rbh_iterator to reclaim
 *
 * \p iterator does not need to be exhausted.
 */
static inline void
rbh_iter_destroy(struct rbh_iterator *iterator)
{
    assert(iterator);
    assert(iterator->ops);
    assert(iterator->ops->destroy);
    return iterator->ops->destroy(iterator);
}

/*----------------------------------------------------------------------------*
 |                              mutable iterator                              |
 *----------------------------------------------------------------------------*/

/**
 * Pointers returned by the `next' method of a mutable iterator are "owned" by
 * the caller and are not cleaned up when the iterator is destroyed.
 */

struct rbh_mut_iterator {
    const struct rbh_mut_iterator_operations *ops;
};

struct rbh_mut_iterator_operations {
    void *(*next)(void *iterator);
    void (*destroy)(void *iterator);
};

/**
 * Yield a mutable reference on the next element of an iterator
 *
 * @param iterator  an iterator
 *
 * @return          a const pointer to the next element in the iterator on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error ENODATA   the iterator is exhausted
 */
static inline void *
rbh_mut_iter_next(struct rbh_mut_iterator *iterator)
{
    assert(iterator);
    assert(iterator->ops);
    assert(iterator->ops->next);
    return iterator->ops->next(iterator);
}

/**
 * Free resources associated to a struct rbh_iterator
 *
 * @param iterator  a pointer to the struct rbh_mut_iterator to reclaim
 *
 * \p iterator does not need to be exhausted.
 */
static inline void
rbh_mut_iter_destroy(struct rbh_mut_iterator *iterator)
{
    assert(iterator);
    assert(iterator->ops);
    assert(iterator->ops->destroy);
    return iterator->ops->destroy(iterator);
}

#endif
