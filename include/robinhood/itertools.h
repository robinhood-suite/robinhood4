/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_ITERTOOLS_H
#define ROBINHOOD_ITERTOOLS_H

#include "robinhood/iterator.h"

/* @file
 * A collection of utilities to manipulate and build iterators
 */

/**
 * Build an iterator from an array
 *
 * @param array         an array
 * @param element_size  the size of the elements in \p array
 * @param element_count the number of elements in \p array
 *
 * @return              a pointer to a newly allocated struct rbh_iterator on
 *                      success, NULL otherwise and errno is set appropriately
 *
 * @retval ENOMEM       there was not enough memory to allocate the iterator
 *
 * The returned iterator's next method yields one element of \p array at a time.
 */
struct rbh_iterator *
rbh_array_iterator(const void *array, size_t element_size,
                   size_t element_count);

/**
 * Build a mutable iterator from an array
 *
 * @param array         an array
 * @param element_size  the size of the elements in \p array
 * @param element_count the number of elements in \p array
 *
 * @return              a pointer to a newly allocated
 *                      struct rbh_mutable_iterator on success, NULL otherwise
 *                      and errno is set appropriately
 *
 * @retval ENOMEM       there was not enough memory to allocate the iterator
 *
 * The returned iterator's next method yields one element of \p array at a time.
 */
struct rbh_mut_iterator *
rbh_mut_array_iterator(void *array, size_t element_size, size_t element_count);

#endif
