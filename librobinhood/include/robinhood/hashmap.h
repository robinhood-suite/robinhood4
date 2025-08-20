/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_HASHMAP_H
#define ROBINHOOD_HASHMAP_H

/**
 * @file
 *
 * Hashmap interface
 *
 * To create a hashmap, use rbh_hashmap_new(). You need to specify a
 * equality-testing and a hash method for the keys, as well as a bound on how
 * many elements the hashmap should be able to contain.
 *
 * Note: the current implementation does not allow for automatic resizing.
 *
 * Example: create a hashmap that can store 4 elements addressed by string keys
 *
 *     static bool
 *     strequals(const void *x, const void *y)
 *     {
 *         return strcmp(x, y) == 0;
 *     }
 *
 *     static size_t
 *     djb2(const void *key)
 *     {
 *         const char *string = key;
 *         unsigned long hash = 5381;
 *         int c;
 *
 *         while ((c = *string++))
 *             hash = ((hash << 5) + hash) + c; // hash * 33 + c
 *
 *         return hash;
 *     }
 *
 *     hashmap = rbh_hashmap_new(strequals, djb2, 4);
 *
 *            key             value
 *     +----------------+----------------+
 *     |                |                |
 *     +----------------+----------------+
 *     |                |                |
 *     +----------------+----------------+
 *     |                |                |
 *     +----------------+----------------+
 *     |                |                |
 *     +----------------+----------------+
 *
 * Keys and values can be arbitrary data structure. A hashmap only keeps
 * references to them, meaning keys must absolutely remain valid until pop-ed,
 * and values probably should, unless you know what you are doing. Of course,
 * a key's hash must be constant, and although its value may change so long as
 * it doesn't conflict with another key in the hashmap, modifying it is in
 * general discouraged.
 *
 * To store a key/value pair in the hashmap, use rbh_hashmap_set().
 *
 * Example: store 3 elements in the hashmap
 *
 *     static const char *KEYS[] = {
 *         "key-0",
 *         "key-1",
 *         "key-2",
 *     };
 *
 *     static const char *VALUES[] = {
 *         "value-0",
 *         "value-1",
 *     };
 *
 *     int rc = rbh_hashmap_set(hashmap, KEYS[0], VALUES[0]);
 *     assert(rc == 0);
 *
 *     rc = rbh_hashmap_set(hashmap, KEYS[1], VALUES[1]);
 *     assert(rc == 0);
*
 *     rc = rbh_hashmap_set(hashmap, KEYS[2], NULL);
 *     assert(rc == 0);
 *
 *            key             value
 *     +----------------+----------------+
 *     |    "key-1"     |   "value-1"    |
 *     +----------------+----------------+
 *     |    "key-2"     |      NULL      |
 *     +----------------+----------------+
 *     |                |                |
 *     +----------------+----------------+
 *     |    "key-0"     |   "value-0"    |
 *     +----------------+----------------+
 *
 * And to query the hashmap for a given key, use rbh_hashmap_get().
 *
 * Example: get a key that is in the hashmap
 *
 *     const void *value = rbh_hashmap_get(hashmap, "key-1");
 *     assert(strcmp(value, "value-0") == 0);
 *
 * Example: get a key that is not in the hashmap
 *
 *     value = rbh_hashmap_get(hashmap, "not-a-key");
 *     assert(value == NULL && errno == ENOENT);
 *
 * If you use NULL pointers for your values, you have to check errno to
 * distinguish successes from errors.
 *
 * Example: get a NULL value from the hashmap
 *
 *     errno = 0
 *     value = rbh_hashmap_get(hashmap, "key-2");
 *     assert(value == NULL && errno = 0);
 *
 * To remove a value from a hashmap, use rbh_hashmap_pop(). A pointer to the
 * corresponding value will be returned on success.
 *
 * Example: remove all the elements in our hashmap
 *
 *     value = rbh_hashmap_pop(hashmap, "key-0");
 *     assert(strcmp(value, "value-0") == 0);
 *
 *     value = rbh_hashmap_pop(hashmap, "key-1");
 *     assert(strcmp(value, "value-1") == 0);
 *
 *     errno = 0;
 *     value = rbh_hashmap_pop(hashmap, "key-2");
 *     assert(value == NULL && errno = 0);
 */

#include <stdbool.h>
#include <stddef.h>

struct rbh_hashmap;

/**
 * Create a hashmap
 *
 * @param equals    a function to compare keys
 * @param hash      a function to hash keys
 * @param count     the number of slots in the hashmap
 *
 * @return          a pointer to a newly allocated hashmap on success, NULL on
 *                  error and errno is set appropriately
 *
 * @error EINVAL    \p count is zero or any of \p equals or \p hash is NULL
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_hashmap *
rbh_hashmap_new(bool (*equals)(const void *first, const void *second),
                size_t (*hash)(const void *key), size_t count);

/**
 * Associate a key to a value in a hashmap
 *
 * @param hashmap   the hashmap in which to store \p key / \p value
 * @param key       the key to associate with \p value
 * @param value     the value to associate with \p key
 *
 * @return          0 on success, -1 on error and errno is set appropriately
 *
 * @error ENOBUFS   there is no more slots available in \p hashmap
 *
 * \p key must remain valid until it is pop-ed or the hashmap is destroyed.
 */
int
rbh_hashmap_set(struct rbh_hashmap *hashmap, const void *key,
                const void *value);

/**
 * Get a value from a key in a hashmap
 *
 * @param hashmap   the hashmap from which to get the value
 * @param key       the key to query \p hashmap with
 *
 * @return          a pointer to the value associated with \p key in \p hashmap
 *                  if there is one, NULL otherwise and errno is set
 *                  appropriately
 *
 * @error ENOENT    \p key is not associated to any value in \p hashmap
 */
const void *
rbh_hashmap_get(struct rbh_hashmap *hashmap, const void *key);

/**
 * Remove a key/value pair from a hashmap
 *
 * @param hashmap   the hashmap from which to pop a key/value pair
 * @param key       the key to pop from \p hashmap
 *
 * @return          a pointer to the value associated with \p key in \p hashmap
 *                  if there was one, NULL otherwise and errno is set
 *                  appropriately
 *
 * @errno ENOENT    \p key is not associated to any value in \p hashmap
 */
const void *
rbh_hashmap_pop(struct rbh_hashmap *hashmap, const void *key);

/**
 * Free resources associated with a hashmap
 *
 * @param hashmap   the hashmap to destroy
 */
void
rbh_hashmap_destroy(struct rbh_hashmap *hashmap);

#endif
