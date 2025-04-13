/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static json_t *
map2json(const struct rbh_value_map *value);

static json_t *
sequence2json(const struct rbh_value *value);

static json_t *
binary2json(const struct rbh_value *value);

static json_t *
value2json(const struct rbh_value *value)
{
    json_t *object;

    if (!value)
        return json_null();

    switch (value->type) {
    case RBH_VT_BOOLEAN:
        object = json_boolean(value->boolean);
        break;
        break;
    case RBH_VT_INT32:
        object = json_integer(value->int32);
        break;
    case RBH_VT_UINT32:
        object = json_integer(value->uint32);
        break;
    case RBH_VT_INT64:
        object = json_integer(value->int64);
        break;
    case RBH_VT_UINT64:
        /* /!\ jansson stores integers as long long (i.e. int64_t) so we cannot
         * store uint64_t values in the xattrs.
         */
        object = json_integer(value->uint64);
        break;
    case RBH_VT_STRING:
        object = json_string(value->string);
        break;
    case RBH_VT_BINARY:
        object = binary2json(value);
        break;
    case RBH_VT_REGEX:
        abort(); /* Regex values are not supposed to be stored in the backend */
        break;
    case RBH_VT_SEQUENCE:
        object = sequence2json(value);
        break;
    case RBH_VT_MAP:
        object = map2json(&value->map);;
        break;
    }

    return object;

}

static json_t *
sequence2json(const struct rbh_value *sequence)
{
    json_t *array = json_array();

    for (size_t i = 0; i < sequence->sequence.count; i++) {
        json_t *elem;

        elem = value2json(&sequence->sequence.values[i]);
        if (!elem) {
            json_decref(array);
            return NULL;
        }

        json_array_append(array, elem);
    }

    return array;
}

void
bin2hex(const struct rbh_value *binary, char *buf)
{
    const void *data = binary->binary.data;
    size_t count = binary->binary.size;

    buf[2 * count] = '\0';
    for (size_t i = 0; i < count; i++) {
        sprintf(&buf[2 * i], "%02x", ((unsigned char *)data)[i]);
    }
}

static json_t *
binary2json(const struct rbh_value *binary)
{
    char *buf;

    buf = malloc(2 * binary->binary.size + 1);
    if (!buf)
        return NULL;

    buf[2 * binary->binary.size] = '\0';
    bin2hex(binary, buf);

    return json_string(buf);
}

static json_t *
create_subobjects(json_t *parent, const char *key)
{
    json_t *subobject;
    char *subkey;
    char *dot;

    dot = strchr(key, '.');
    if (!dot)
        return parent;

    subkey = strndup(key, dot - key);
    subobject = json_object_get(parent, subkey);
    if (!subobject) {
        subobject = json_object();
        if (!subobject) {
            free(subkey);
            return NULL;
        }
    }

    json_object_set(parent, subkey, subobject);
    free(subkey);
    return create_subobjects(subobject, dot + 1);
}

static const char *
last_key(const char *key)
{
    char *last_dot = strrchr(key, '.');

    if (last_dot)
        return last_dot + 1;

    return key;
}

static json_t *
map2json(const struct rbh_value_map *map)
{
    json_t *object;

    object = json_object();
    if (!object)
        return NULL;

    for (size_t i = 0; i < map->count; i++) {
        json_t *subobject;
        json_t *value;

        value = value2json(map->pairs[i].value);
        if (!value) {
            json_decref(object);
            return NULL;
        }

        subobject = create_subobjects(object, map->pairs[i].key);
        if (!subobject) {
            json_decref(object);
            return NULL;
        }

        json_object_set(subobject, last_key(map->pairs[i].key), value);
    }

    return object;
}

const char *
sqlite_xattr2json(const struct rbh_value_map *xattrs)
{
    const char *repr;
    json_t *object;

    object = map2json(xattrs);
    if (!object)
        return NULL;

    repr = json_dumps(object, JSON_COMPACT);
    json_decref(object);

    return repr;
}
