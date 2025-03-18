/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>

#include "robinhood/value.h"
#include "robinhood/utils.h"

#include "mongo.h"
#include "utils.h"

/*----------------------------------------------------------------------------*
 |                          bson_append_rbh_value()                           |
 *----------------------------------------------------------------------------*/

static bool
bson_append_rbh_value_sequence(bson_t *bson, const char *key, size_t key_length,
                               const struct rbh_value *values, size_t count)
{
    bson_t array;

    if (!bson_append_array_begin(bson, key, key_length, &array))
        return false;

    for (uint32_t i = 0; i < count; i++) {
        char str[16];

        key_length = bson_uint32_to_string(i, &key, str, sizeof(str));
        if (!bson_append_rbh_value(&array, key, key_length, &values[i]))
            return false;
    }

    return bson_append_array_end(bson, &array);
}

static bool
_bson_append_regex(bson_t *bson, const char *key, size_t key_length,
                   const char *regex, unsigned int options)
{
    char mongo_regex_options[8] = {'s', '\0',};
    char *pcre = NULL;
    uint8_t i = 1;

    /* If it's a shell pattern, we need to convert to PCRE for mongo */
    if (options & RBH_RO_SHELL_PATTERN) {
        pcre = shell2pcre(regex);
        if (pcre == NULL)
            error_at_line(EXIT_FAILURE, ENOMEM, __FILE__, __LINE__ - 2,
                          "converting %s into a Perl Compatible Regular "
                          "Expression", regex);
    }

    if (options & RBH_RO_CASE_INSENSITIVE)
        mongo_regex_options[i++] = 'i';

    return bson_append_regex(bson, key, key_length, pcre == NULL ? regex : pcre,
                             mongo_regex_options);
}

bool
bson_append_rbh_value(bson_t *bson, const char *key, size_t key_length,
                      const struct rbh_value *value)
{
    if (!value)
        return bson_append_null(bson, key, key_length);

    switch (value->type) {
    case RBH_VT_BOOLEAN:
        return bson_append_bool(bson, key, key_length, value->boolean);
    case RBH_VT_INT32:
        return bson_append_int32(bson, key, key_length, value->int32);
    case RBH_VT_UINT32:
        return bson_append_int32(bson, key, key_length, value->uint32);
    case RBH_VT_INT64:
        return bson_append_int64(bson, key, key_length, value->int64);
    case RBH_VT_UINT64:
        return bson_append_int64(bson, key, key_length, value->uint64);
    case RBH_VT_STRING:
        return bson_append_utf8(bson, key, key_length, value->string,
                                strlen(value->string));
    case RBH_VT_BINARY:
        return _bson_append_binary(bson, key, key_length, BSON_SUBTYPE_BINARY,
                                   value->binary.data, value->binary.size);
    case RBH_VT_REGEX:
        return _bson_append_regex(bson, key, key_length, value->regex.string,
                                  value->regex.options);
    case RBH_VT_SEQUENCE:
        return bson_append_rbh_value_sequence(bson, key, key_length,
                                              value->sequence.values,
                                              value->sequence.count);
    case RBH_VT_MAP:
        /* TODO: support this */
        return bson_append_rbh_value_map(bson, key, key_length, &value->map);
    }
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                        bson_append_rbh_value_map()                         |
 *----------------------------------------------------------------------------*/

bool
bson_append_rbh_value_map(bson_t *bson, const char *key, size_t keylen,
                          const struct rbh_value_map *map)
{
    bson_t document;

    if (!bson_append_document_begin(bson, key, keylen, &document))
        return false;

    for (size_t i = 0; i < map->count; i++) {
        const struct rbh_value_pair *pair = &map->pairs[i];

        if (!BSON_APPEND_RBH_VALUE(&document, pair->key, pair->value))
            return false;
    }

    return bson_append_document_end(bson, &document);
}

    /*--------------------------------------------------------------------*
     |                     bson_iter_rbh_value_map()                      |
     *--------------------------------------------------------------------*/

bool
bson_iter_rbh_value_map(bson_iter_t *iter, struct rbh_value_map *map,
                        size_t count, char **buffer, size_t *bufsize)
{
    struct rbh_value_pair *pairs;
    struct rbh_value *values;
    size_t size = *bufsize;
    char *data = *buffer;

    pairs = aligned_memalloc(alignof(*pairs), count * sizeof(*pairs), &data,
                             &size);
    if (pairs == NULL)
        return false;

    values = aligned_memalloc(alignof(*values), count * sizeof(*values), &data,
                              &size);
    if (values == NULL)
        return false;

    map->pairs = pairs;
    while (bson_iter_next(iter)) {
        if (!bson_iter_rbh_value(iter, values, &data, &size)) {
            if (errno == ENOTSUP)
                /* Ignore */
                continue;
            return false;
        }

        pairs->key = bson_iter_key(iter);
        pairs->value = values++;
        pairs++;
    }
    map->count = count;

    *buffer = data;
    *bufsize = size;
    return true;
}

    /*--------------------------------------------------------------------*
     |                       bson_iter_rbh_value()                        |
     *--------------------------------------------------------------------*/

static bool
bson_iter_rbh_value_sequence(bson_iter_t *iter, struct rbh_value *value,
                             size_t count, char **buffer, size_t *bufsize)
{
    struct rbh_value *values;
    size_t size = *bufsize;
    char *data = *buffer;

    value->type = RBH_VT_SEQUENCE;
    values = aligned_memalloc(alignof(*values), count * sizeof(*values), &data,
                              &size);
    if (values == NULL)
        return false;

    value->sequence.values = values;
    while (bson_iter_next(iter)) {
        if (!bson_iter_rbh_value(iter, values, &data, &size)) {
            if (errno == ENOTSUP)
                /* Ignore */
                continue;
            return false;
        }
        values++;
    }
    value->sequence.count = count;

    *buffer = data;
    *bufsize = size;
    return true;
}

bool
bson_iter_rbh_value(bson_iter_t *iter, struct rbh_value *value,
                    char **buffer, size_t *bufsize)
{
    bson_iter_t subiter, tmp;
    const uint8_t *data;
    uint32_t size;

    switch (bson_iter_type(iter)) {
    case BSON_TYPE_UTF8:
        value->type = RBH_VT_STRING;
        value->string = bson_iter_utf8(iter, NULL);
        break;
    case BSON_TYPE_DOCUMENT:
        value->type = RBH_VT_MAP;
        bson_iter_recurse(iter, &tmp);
        bson_iter_recurse(iter, &subiter);
        if (!bson_iter_rbh_value_map(&subiter, &value->map,
                                     bson_iter_count(&tmp), buffer, bufsize))
            return false;
        break;
    case BSON_TYPE_ARRAY:
        value->type = RBH_VT_SEQUENCE;
        bson_iter_recurse(iter, &tmp);
        bson_iter_recurse(iter, &subiter);
        /* We cannot pass just `&value->sequence' as it is an unnamed struct */
        if (!bson_iter_rbh_value_sequence(&subiter, value,
                                          bson_iter_count(&tmp), buffer,
                                          bufsize))
            return false;
        break;
    case BSON_TYPE_BINARY:
        value->type = RBH_VT_BINARY;
        bson_iter_binary(iter, NULL, &size, &data);
        value->binary.data = (const char *)data;
        value->binary.size = size;
        break;
    case BSON_TYPE_BOOL:
        value->type = RBH_VT_BOOLEAN;
        value->boolean = bson_iter_bool(iter);
        break;
    case BSON_TYPE_NULL:
        value->type = RBH_VT_BINARY;
        value->binary.size = 0;
        break;
    case BSON_TYPE_REGEX:
        value->type = RBH_VT_REGEX;
        /* TODO: convert mongo regex options */
        errno = ENOTSUP;
        return false;
        break;
    case BSON_TYPE_INT32:
        value->type = RBH_VT_INT32;
        value->int32 = bson_iter_int32(iter);
        break;
    case BSON_TYPE_DOUBLE:
        /* Handle floating types as truncated integers, maybe we'll have to add
         * the proper float type for rbh-value, but right now that's good enough
         */
        value->type = RBH_VT_INT64;
        value->int64 = bson_iter_double(iter);
        break;
    case BSON_TYPE_INT64:
        value->type = RBH_VT_INT64;
        value->int64 = bson_iter_int64(iter);
        break;
    default:
        errno = ENOTSUP;
        return false;
    }
    return true;
}
