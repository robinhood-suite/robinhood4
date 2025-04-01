/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_VALUE_H
#define RBH_VALUE_H

#include "robinhood/sstack.h"
#include "robinhood/value.h"

/** @file
 * A few helpers around struct rbh_value to be used internally
 */

/**
 * Compute the size of the data a value points at
 *
 * @param value     the value whose data size to compute
 * @param offset    an alignment offset
 *
 * @return          the complement of \p offset + the number of bytes \p value
 *                  points at on success, -1 on error and errno is set
 *                  appropriately
 *
 * @error EINVAL    \p value's type is invalid
 */
ssize_t __attribute__((pure))
value_data_size(const struct rbh_value *value, size_t offset);

/**
 * Make a standalone copy of a value
 *
 * @param dest      the value to copy to
 * @param src       the value to copy from
 * @param buffer    a buffer of at least \p bufsize bytes where any data \p src
 *                  references will be copied, on success it is updated to point
 *                  after the last byte that was copied in it
 * @param bufsize   a pointer to the number of bytes after \p buffer that are
 *                  valid, on success the value it points at is decremented by
 *                  the number of bytes that were written to \p data.
 *
 * @return          0 on success, -1 on error and errno is set appropriately
 *
 * @error EINVAL    \p src's type is invalid
 * @error ENOBUFS   \p size is not big enough to store all the data \p src
 *                  points at
 *
 * After a successful return, \p dest and \p src do not share any of the data
 * they point at.
 */
int
value_copy(struct rbh_value *dest, const struct rbh_value *src, char **buffer,
           size_t *bufsize);

/**
 * Clone the given \p value
 *
 * @param value     the value to clone
 *
 * @return          the cloned value on success, NULL on error and errno is set
 *                  appropriately
 */
struct rbh_value *
value_clone(const struct rbh_value *value);

/**
 * Compute the size of the data a map points at
 *
 * @param map       the map whose data size to compute
 *
 * @return          the number of bytes \p map points at on success, -1 on error
 *                  and errno is set appropriately
 *
 * @error EINVAL    \p map points at invalid data (eg. a struct rbh_value with
 *                  an incorrect type)
 */
ssize_t
value_map_data_size(const struct rbh_value_map *map);

/**
 * Fill a given value pair with the key and int64_t provided using a sstack for
 * allocation.
 *
 * @param  key      the key to fill in the rbh_value_pair
 * @param  integer  the value to fill in the rbh_value_pair
 * @param  pair     the rbh_value_pair to fill
 * @param  stack    the sstack to use for allocations
 *
 * @return 0 on success, -1 on error
 */
int
fill_int64_pair(const char *key, int64_t integer, struct rbh_value_pair *pair,
                struct rbh_sstack *stack);

/**
 * Fill a given value pair with the key and string provided using a sstack for
 * allocation.
 *
 * @param  key      the key to fill in the rbh_value_pair
 * @param  str      the value to fill in the rbh_value_pair
 * @param  pair     the rbh_value_pair to fill
 * @param  stack    the sstack to use for allocations
 *
 * @return 0 on success, -1 on error
 */
int
fill_string_pair(const char *key, const char *str, struct rbh_value_pair *pair,
                 struct rbh_sstack *stack);

/**
 * Fill a given value pair with the key and binary data provided using a sstack
 * for allocation.
 *
 * @param  key      the key to fill in the rbh_value_pair
 * @param  data     the value to fill in the rbh_value_pair
 * @param  len      the length of the value to fill in the rbh_value_pair
 * @param  pair     the rbh_value_pair to fill
 * @param  stack    the sstack to use for allocations
 *
 * @return 0 on success, -1 on error
 */
int
fill_binary_pair(const char *key, const void *data, const size_t len,
                 struct rbh_value_pair *pair, struct rbh_sstack *stack);

/**
 * Fill a given value pair with the key and int32 provided using a sstack for
 * allocation.
 *
 * @param  key      the key to fill in the rbh_value_pair
 * @param  integer  the value to fill in the rbh_value_pair
 * @param  pair     the rbh_value_pair to fill
 * @param  stack    the sstack to use for allocations
 *
 * @return 0 on success, -1 on error
 */
int
fill_int32_pair(const char *key, int32_t integer, struct rbh_value_pair *pair,
                struct rbh_sstack *stack);

/**
 * Fill a given value pair with the key and uint32 provided using a sstack for
 * allocation.
 *
 * @param  key      the key to fill in the rbh_value_pair
 * @param  integer  the value to fill in the rbh_value_pair
 * @param  pair     the rbh_value_pair to fill
 * @param  stack    the sstack to use for allocations
 *
 * @return 0 on success, -1 on error
 */
int
fill_uint32_pair(const char *key, uint32_t integer, struct rbh_value_pair *pair,
                 struct rbh_sstack *stack);

/**
 * Fill a given value pair with the sequence of rbh_values provided using a
 * sstack for allocation.
 *
 * @param  key      the key to fill in the rbh_value_pair
 * @param  values   the values which make up the sequence to fill in the
 *                  rbh_value_pair
 * @param  length   the number of values in the sequence to fill in the
 *                  rbh_value_pair
 * @param  pair     the rbh_value_pair to fill
 * @param  stack    the sstack to use for allocations
 *
 * @return 0 on success, -1 on error
 */
int
fill_sequence_pair(const char *key, struct rbh_value *values, uint64_t length,
                   struct rbh_value_pair *pair, struct rbh_sstack *stack);

#endif
