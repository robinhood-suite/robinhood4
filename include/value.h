/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef RBH_VALUE_H
#define RBH_VALUE_H

/** @file
 * A few helpers around struct rbh_value to be used internally
 */

/**
 * Compute the size of the data a value points at
 *
 * @param value     the value whose data size to compute
 *
 * @return          the number of bytes \p value points at on success, -1 on
 *                  error and errno is set appropriately
 *
 * @error EINVAL    \p value's type is invalid
 */
ssize_t __attribute__((pure))
value_data_size(const struct rbh_value *value);

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
 * Make a standalone copy of a map
 *
 * @param dest      the map to copy to
 * @param src       the map to copy from
 * @param buffer    a buffer of at least \p bufsize bytes where any data \p src
 *                  references will be copied, on success it is updated to point
 *                  after the last byte that was copied in it
 * @param bufsize   a pointer to the number of bytes after \p buffer that are
 *                  valid, on success the value it points at is decremented by
 *                  the number of bytes that were written to \p data.
 *
 * @return          0 on success, -1 on error and errno is set appropriately
 *
 * @error EINVAL    \p src points at invalid data (eg. a struct rbh_value with
 *                  an incorrect type)
 * @error ENOBUFS   \p size is not big enough to store all the data \p src
 *                  points at
 *
 * After a successful return, \p dest and \p src do not share any of the data
 * they point at.
 */
int
value_map_copy(struct rbh_value_map *dest, const struct rbh_value_map *src,
               char **buffer, size_t *bufsize);

#endif
