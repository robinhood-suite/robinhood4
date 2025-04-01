/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "robinhood/value.h"
#include "utils.h"
#include "value.h"

ssize_t __attribute__((pure))
value_data_size(const struct rbh_value *value, size_t offset)
{
    size_t size;

    switch (value->type) {
    case RBH_VT_BOOLEAN:
    case RBH_VT_INT32:
    case RBH_VT_UINT32:
    case RBH_VT_INT64:
    case RBH_VT_UINT64:
        return 0;
    case RBH_VT_STRING:
        return strlen(value->string) + 1;
    case RBH_VT_BINARY:
        return value->binary.size;
    case RBH_VT_REGEX:
        return strlen(value->regex.string) + 1;
    case RBH_VT_SEQUENCE:
        offset = sizealign(offset, alignof(*value->sequence.values)) - offset;
        size = value->sequence.count * sizeof(*value->sequence.values);
        for (size_t i = 0; i < value->sequence.count; i++) {
            size = sizealign(size, alignof(*value));
            if (value_data_size(&value->sequence.values[i], size) < 0)
                return -1;
            size += value_data_size(&value->sequence.values[i], size);
        }
        return offset + size;
    case RBH_VT_MAP:
        offset = sizealign(offset, alignof(*value->map.pairs)) - offset;
        return offset + value_map_data_size(&value->map);
    }

    errno = EINVAL;
    return -1;
}

static ssize_t __attribute__((pure))
value_pair_data_size(const struct rbh_value_pair *pair)
{
    size_t size;

    /* pair->key */
    size = strlen(pair->key) + 1;

    /* pair->value */
    if (pair->value == NULL)
        return size;

    size = sizealign(size, alignof(*pair->value));
    size += sizeof(*pair->value);
    if (value_data_size(pair->value, size) < 0)
        return -1;
    size += value_data_size(pair->value, size);

    return size;
}

ssize_t __attribute__((pure))
value_map_data_size(const struct rbh_value_map *map)
{
    size_t size;

    /* map->pairs */
    size = map->count * sizeof(*map->pairs);
    for (size_t i = 0; i < map->count; i++) {
        const struct rbh_value_pair *pair = &map->pairs[i];

        size = sizealign(size, alignof(*pair));
        if (value_pair_data_size(pair) < 0)
            return -1;
        size += value_pair_data_size(pair);
    }

    return size;
}

int
value_copy(struct rbh_value *dest, const struct rbh_value *src, char **buffer,
           size_t *bufsize)
{
    struct rbh_value *values;
    size_t size = *bufsize;
    char *data = *buffer;
    size_t length;

    /* dest->type */
    dest->type = src->type;

    switch (src->type) {
    case RBH_VT_BOOLEAN: /* dest->boolean */
        dest->boolean = src->boolean;
        break;
    case RBH_VT_INT32: /* dest->int32 */
        dest->int32 = src->int32;
        break;
    case RBH_VT_UINT32: /* dest->uint32 */
        dest->uint32 = src->uint32;
        break;
    case RBH_VT_INT64: /* dest->int64 */
        dest->int64 = src->int64;
        break;
    case RBH_VT_UINT64: /* dest->uint64 */
        dest->uint64 = src->uint64;
        break;
    case RBH_VT_STRING: /* dest->string */
        length = strlen(src->string) + 1;
        if (size < length)
            goto out_enobufs;

        dest->string = data;
        data = mempcpy(data, src->string, length);
        size -= length;
        break;
    case RBH_VT_BINARY: /* dest->binary */
        /* dest->binary.data */
        if (size < src->binary.size)
            goto out_enobufs;

        dest->binary.data = data;
        if (src->binary.size > 0)
            data = mempcpy(data, src->binary.data, src->binary.size);
        size -= src->binary.size;

        /* dest->binary.size */
        dest->binary.size = src->binary.size;
        break;
    case RBH_VT_REGEX: /* dest->regex */
        /* dest->regex.string */
        length = strlen(src->regex.string) + 1;
        if (size < length)
            goto out_enobufs;

        dest->regex.string = data;
        data = mempcpy(data, src->regex.string, length);
        size -= length;

        /* dest->regex.options */
        dest->regex.options = src->regex.options;
        break;
    case RBH_VT_SEQUENCE: /* dest->sequence */
        /* dest->sequence.values */
        values = aligned_memalloc(alignof(*values),
                                  src->sequence.count * sizeof(*values), &data,
                                  &size);
        if (values == NULL)
            return -1;

        for (size_t i = 0; i < src->sequence.count; i++) {
            if (value_copy(&values[i], &src->sequence.values[i], &data, &size))
                return -1;
        }
        dest->sequence.values = values;

        /* dest->sequence.count */
        dest->sequence.count = src->sequence.count;
        break;
    case RBH_VT_MAP: /* dest->map */
        if (value_map_copy(&dest->map, &src->map, &data, &size))
            return -1;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    *buffer = data;
    *bufsize = size;
    return 0;

out_enobufs:
    errno = ENOBUFS;
    return -1;
}

static int
value_pair_copy(struct rbh_value_pair *dest, const struct rbh_value_pair *src,
                char **buffer, size_t *bufsize)
{
    struct rbh_value *value;
    size_t size = *bufsize;
    char *data = *buffer;
    size_t keylen;

    /* dest->key */
    keylen = strlen(src->key) + 1;
    if (size < keylen)
        goto out_enobufs;

    dest->key = data;
    data = mempcpy(data, src->key, keylen);
    size -= keylen;

    /* dest->value */
    if (src->value == NULL) {
        dest->value = NULL;
        goto out;
    }

    value = aligned_memalloc(alignof(*value), sizeof(*value), &data, &size);
    if (value == NULL)
        return -1;
    if (value_copy(value, src->value, &data, &size))
        return -1;
    dest->value = value;

out:
    *buffer = data;
    *bufsize = size;
    return 0;

out_enobufs:
    errno = ENOBUFS;
    return -1;
}

int
value_map_copy(struct rbh_value_map *dest, const struct rbh_value_map *src,
               char **buffer, size_t *bufsize)
{
    struct rbh_value_pair *pairs;
    size_t size = *bufsize;
    char *data = *buffer;

    /* dest->pairs */
    pairs = aligned_memalloc(alignof(*pairs), src->count * sizeof(*pairs),
                             &data, &size);
    if (pairs == NULL)
        return -1;

    for (size_t i = 0; i < src->count; i++) {
        if (value_pair_copy(&pairs[i], &src->pairs[i], &data, &size))
            return -1;
    }
    dest->pairs = pairs;

    /* dest->count */
    dest->count = src->count;

    *buffer = data;
    *bufsize = size;
    return 0;
}

struct rbh_value *
value_clone(const struct rbh_value *value)
{
    struct rbh_value *clone;
    size_t size;
    char *data;
    int rc;

    size = value_data_size(value, 0);
    clone = malloc(sizeof(*clone) + size);
    if (clone == NULL)
        return NULL;
    data = (char *)clone + sizeof(*clone);

    rc = value_copy(clone, value, &data, &size);
    assert(rc == 0);

    return clone;
}

struct rbh_value *
rbh_value_boolean_new(bool boolean)
{
    const struct rbh_value boolean_ = {
        .type = RBH_VT_BOOLEAN,
        .int32 = boolean,
    };

    return value_clone(&boolean_);
}

struct rbh_value *
rbh_value_int32_new(int32_t int32)
{
    const struct rbh_value int32_ = {
        .type = RBH_VT_INT32,
        .int32 = int32,
    };

    return value_clone(&int32_);
}

struct rbh_value *
rbh_value_uint32_new(uint32_t uint32)
{
    const struct rbh_value uint32_ = {
        .type = RBH_VT_UINT32,
        .uint32 = uint32,
    };

    return value_clone(&uint32_);
}

struct rbh_value *
rbh_value_int64_new(int64_t int64)
{
    const struct rbh_value int64_ = {
        .type = RBH_VT_INT64,
        .int64 = int64,
    };

    return value_clone(&int64_);
}

struct rbh_value *
rbh_value_uint64_new(uint64_t uint64)
{
    const struct rbh_value uint64_ = {
        .type = RBH_VT_UINT64,
        .uint64 = uint64,
    };

    return value_clone(&uint64_);
}

struct rbh_value *
rbh_value_string_new(const char *string)
{
    const struct rbh_value string_ = {
        .type = RBH_VT_STRING,
        .string = string,
    };

    return value_clone(&string_);
}

struct rbh_value *
rbh_value_binary_new(const char *data, size_t size)
{
    const struct rbh_value binary = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = data,
            .size = size,
        },
    };

    return value_clone(&binary);
}

struct rbh_value *
rbh_value_regex_new(const char *regex, unsigned int options)
{
    const struct rbh_value regex_ = {
        .type = RBH_VT_REGEX,
        .regex = {
            .string = regex,
            .options = options,
        },
    };

    if (options & ~RBH_RO_ALL) {
        errno = EINVAL;
        return NULL;
    }

    return value_clone(&regex_);
}

struct rbh_value *
rbh_value_sequence_new(const struct rbh_value *values, size_t count)
{
    const struct rbh_value sequence = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = values,
            .count = count,
        },
    };

    return value_clone(&sequence);
}

struct rbh_value *
rbh_value_map_new(const struct rbh_value_pair *pairs, size_t count)
{
    const struct rbh_value map = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = pairs,
            .count = count,
        },
    };

    return value_clone(&map);
}

int
rbh_value_validate(const struct rbh_value *value)
{
    switch (value->type) {
    case RBH_VT_BOOLEAN:
    case RBH_VT_INT32:
    case RBH_VT_UINT32:
    case RBH_VT_INT64:
    case RBH_VT_UINT64:
        return 0;
    case RBH_VT_STRING:
        if (value->string == NULL)
            goto out_einval;
        return 0;
    case RBH_VT_BINARY:
        if (value->binary.size != 0 && value->binary.data == NULL)
            goto out_einval;
        return 0;
    case RBH_VT_REGEX:
        if (value->regex.string == NULL || value->regex.options & ~RBH_RO_ALL)
            goto out_einval;
        return 0;
    case RBH_VT_SEQUENCE:
        if (value->sequence.count != 0 && value->sequence.values == NULL)
            goto out_einval;
        for (size_t i = 0; i < value->sequence.count; i++) {
            if (rbh_value_validate(&value->sequence.values[i]))
                return -1;
        }
        return 0;
    case RBH_VT_MAP:
        if (value->map.count != 0 && value->map.pairs == NULL)
            goto out_einval;
        for (size_t i = 0; i < value->map.count; i++) {
            if (value->map.pairs[i].key == NULL)
                goto out_einval;
            if (value->map.pairs[i].value == NULL)
                goto out_einval;
            if (rbh_value_validate(value->map.pairs[i].value))
                return -1;
        }
        return 0;
    }

out_einval:
    errno = EINVAL;
    return -1;
}

static int
fill_pair(const char *key, const struct rbh_value *value,
          struct rbh_value_pair *pair, struct rbh_sstack *stack)
{
    pair->key = key;
    pair->value = rbh_sstack_push(stack, value, sizeof(*value));
    if (pair->value == NULL)
        return -1;

    return 0;
}

int
fill_int64_pair(const char *key, int64_t integer, struct rbh_value_pair *pair,
                struct rbh_sstack *stack)
{
    const struct rbh_value int64_value = {
        .type = RBH_VT_INT64,
        .int64 = integer,
    };

    return fill_pair(key, &int64_value, pair, stack);
}

int
fill_string_pair(const char *key, const char *str,
                 struct rbh_value_pair *pair, struct rbh_sstack *stack)
{
    const struct rbh_value string_value = {
        .type = RBH_VT_STRING,
        .string = rbh_sstack_push(stack, str, strlen(str) + 1),
    };

    if (string_value.string == NULL)
        return -1;

    return fill_pair(key, &string_value, pair, stack);
}

int
fill_binary_pair(const char *key, const void *data, const size_t len,
                 struct rbh_value_pair *pair, struct rbh_sstack *stack)
{
    const struct rbh_value binary_value = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = rbh_sstack_push(stack, data, len),
            .size = len,
        },
    };

    if (binary_value.binary.data == NULL)
        return -1;

    return fill_pair(key, &binary_value, pair, stack);
}

int
fill_int32_pair(const char *key, int32_t integer, struct rbh_value_pair *pair,
                struct rbh_sstack *stack)
{
    const struct rbh_value int32_value = {
        .type = RBH_VT_INT32,
        .int32 = integer,
    };

    return fill_pair(key, &int32_value, pair, stack);
}

int
fill_uint32_pair(const char *key, uint32_t integer, struct rbh_value_pair *pair,
                 struct rbh_sstack *stack)
{
    const struct rbh_value uint32_value = {
        .type = RBH_VT_UINT32,
        .uint32 = integer,
    };

    return fill_pair(key, &uint32_value, pair, stack);
}

int
fill_sequence_pair(const char *key, struct rbh_value *values, uint64_t length,
                   struct rbh_value_pair *pair, struct rbh_sstack *stack)
{
    const struct rbh_value sequence_value = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = rbh_sstack_push(stack, values,
                                      length * sizeof(*values)),
            .count = length,
        },
    };

    if (sequence_value.sequence.values == NULL)
        return -1;

    return fill_pair(key, &sequence_value, pair, stack);
}
