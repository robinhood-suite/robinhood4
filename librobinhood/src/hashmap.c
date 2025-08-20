/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include "robinhood/hashmap.h"

struct rbh_hashmap_item {
    const void *key;
    const void *value;
};

struct rbh_hashmap {
    size_t (*hash)(const void *key);
    bool (*equals)(const void *first, const void *second);
    struct rbh_hashmap_item *items;
    size_t count;
};

struct rbh_hashmap *
rbh_hashmap_new(bool (*equals)(const void *first, const void *second),
                size_t (*hash)(const void *key), size_t count)
{
    struct rbh_hashmap *hashmap;

    if (count == 0 || equals == NULL || hash == NULL) {
        errno = EINVAL;
        return NULL;
    }

    hashmap = malloc(sizeof(*hashmap));
    if (hashmap == NULL)
        return NULL;

    hashmap->items = reallocarray(NULL, count, sizeof(*hashmap->items));
    if (hashmap->items == NULL) {
        int save_errno = errno;

        free(hashmap);
        errno = save_errno;
        return NULL;
    }

    hashmap->hash = hash;
    hashmap->equals = equals;
    for (size_t i = 0; i < count; i++)
        hashmap->items[i].key = NULL;

    hashmap->count = count;
    return hashmap;
}

static size_t __attribute__((pure))
_hashmap_key2slot(struct rbh_hashmap *hashmap, const void *key)
{
    return hashmap->hash(key) % hashmap->count;
}

/* Basic open addressing implementation */
static struct rbh_hashmap_item *
hashmap_key2slot(struct rbh_hashmap *hashmap, const void *key)
{
    size_t index = _hashmap_key2slot(hashmap, key);

    for (size_t i = index; i < hashmap->count; i++) {
        struct rbh_hashmap_item *item = &hashmap->items[i];

        if (item->key == NULL || hashmap->equals(item->key, key))
            return item;
    }

    for (size_t i = 0; i < index; i++) {
        struct rbh_hashmap_item *item = &hashmap->items[i];

        if (item->key == NULL || hashmap->equals(item->key, key))
            return item;
    }

    return NULL;
}

int
rbh_hashmap_set(struct rbh_hashmap *hashmap, const void *key, const void *value)
{
    struct rbh_hashmap_item *item;

    item = hashmap_key2slot(hashmap, key);
    if (item == NULL) {
        errno = ENOBUFS;
        return -1;
    }

    item->key = key;
    item->value = value;
    return 0;
}

const void *
rbh_hashmap_get(struct rbh_hashmap *hashmap, const void *key)
{
    struct rbh_hashmap_item *match = hashmap_key2slot(hashmap, key);

    if (match == NULL || match->key == NULL) {
        errno = ENOENT;
        return NULL;
    }

    return match->value;
}

/* Here, "is_between" means: if we traverse an array starting at index `low`,
 * wrap around once we reach the end, and stop at `high`, do we encounter
 * `index`?
 */
static bool __attribute__((pure))
is_between(size_t index, size_t low, size_t high)
{
    return low <= high ? low <= index && index <= high
                       : low <= index || index <= high;
}

/* Since we use open-addressing with linear probing, we can't just empty a slot
 * and be done with it. We have to maintain the invariant, which means
 * traversing the array looking for a cell that would be better placed in the
 * slot we are emptying, and move the first one we find. We can stop at the
 * first empty cell we encounter.
 *
 * Of course, if we have to move a cell, it just frees another slot, and we have
 * to enforce the invariant again.
 *
 * To the question:
 *     When would a cell be better placed in a slot we are emptying?
 *
 * The answer is:
 *     Iff the ideal position for the cell is "before" the slot we are emptying.
 *
 * I personally find it hard to grasp what "before" means when we reach the end
 * of the array and have to wrap around. I prefer the following definition:
 *
 * A cell should be moved when the slot we empty is "between" the cell's ideal
 * position and its current position:
 *
 * Example:
 *
 *                   the slot we are emptying
 *                   v
 *     ---------------------------------
 *     | x |   | i | e | x | x | c |   |
 *     ---------------------------------
 *               ^               ^
 *               |               the cell we are considering
 *               |
 *               the cell's ideal position
 *
 *     We indeed have i < e < c
 *
 * Real-world example:
 *
 *     Assuming a filled hashmap, where keys are chars, and the hash algorithm
 *     is: hash(x) -> x - 'A'
 *
 *       0   1   2   3   4   5   6   7
 *     ---------------------------------
 *     | A | B | K | D | C | E | F | G |  <-- keys
 *     ---------------------------------
 *     |...|...|...|...|...|...|...|...|  <-- values (not relevant here)
 *     ---------------------------------
 *
 *     If we pop 'D', and do nothing else:
 *
 *       0   1   2   3   4   5   6   7
 *     ---------------------------------
 *     | A | B | K |   | C | E | F | G |
 *     ---------------------------------
 *
 *     We can't find 'C' anymore:
 *
 *     hash('C') -> 2
 *     items[2] -> 'K'   # not a match => keep looking
 *     items[3] -> NULL  # empty slot  => stop looking
 *
 *     We have to move 'C' back:
 *
 *       0   1   2   3   4   5   6   7
 *     ---------------------------------
 *     | A | B | K | C |   | E | F | G |
 *     ---------------------------------
 *
 *     And then 'E':
 *
 *       0   1   2   3   4   5   6   7
 *     ---------------------------------
 *     | A | B | K | C | E |   | F | G |
 *     ---------------------------------
 *
 *     And then 'F', and then 'G', until finally:
 *
 *       0   1   2   3   4   5   6   7
 *     ---------------------------------
 *     | A | B | K | C | E | F | G |   |
 *     ---------------------------------
 *
 *     Order is restored. =)
 *
 * ref: https://en.wikipedia.org/wiki/Linear_probing#Deletion
 */
static void
hashmap_pop(struct rbh_hashmap *hashmap, struct rbh_hashmap_item *item)
{
    size_t index = item - hashmap->items;

    for (size_t i = index + 1; i < hashmap->count; i++) {
        struct rbh_hashmap_item *next = &hashmap->items[i];

        if (next->key == NULL)
            goto out;

        if (is_between(index, _hashmap_key2slot(hashmap, next->key), i)) {
            *item = *next;
            return hashmap_pop(hashmap, next);
        }
    }

    for (size_t i = 0; i < index; i++) {
        struct rbh_hashmap_item *next = &hashmap->items[i];

        if (next->key == NULL)
            goto out;

        if (is_between(index, _hashmap_key2slot(hashmap, next->key), i)) {
            *item = *next;
            return hashmap_pop(hashmap, next);
        }
    }

out:
    item->key = NULL;
}

const void *
rbh_hashmap_pop(struct rbh_hashmap *hashmap, const void *key)
{
    struct rbh_hashmap_item *match = hashmap_key2slot(hashmap, key);
    const void *value;

    if (match == NULL || match->key == NULL) {
        errno = ENOENT;
        return NULL;
    }
    value = match->value;

    hashmap_pop(hashmap, match);
    return value;
}

void
rbh_hashmap_destroy(struct rbh_hashmap *hashmap)
{
    free(hashmap->items);
    free(hashmap);
}
