/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/value.h"

#include "mongo.h"

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
    uint8_t i = 1;

    if (options & RBH_RO_CASE_INSENSITIVE)
        mongo_regex_options[i++] = 'i';

    return bson_append_regex(bson, key, key_length, regex, mongo_regex_options);
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

