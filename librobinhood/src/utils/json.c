/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <assert.h>
#include <jansson.h>

#include "robinhood/utils.h"

static bool
json2value(json_t *object, struct rbh_value *value,
           struct rbh_sstack *sstack);

json_t *
map2json(const struct rbh_value_map *value, struct rbh_sstack *sstack);

static json_t *
sequence2json(const struct rbh_value *value, struct rbh_sstack *sstack);

static json_t *
binary2json(const struct rbh_value *value, struct rbh_sstack *sstack);

static bool
json_array2sequence(json_t *object, struct rbh_value *value,
                    struct rbh_sstack *sstack)
{
    struct rbh_value *values;
    json_t *elem;
    size_t index;

    value->sequence.count = json_array_size(object);
    values = rbh_sstack_alloc(sstack, NULL,
                              sizeof(*values) * value->sequence.count);
    if (!values)
        return false;

    json_array_foreach(object, index, elem) {
        if (!json2value(elem, &values[index], sstack))
            return false;
    }

    value->sequence.values = values;

    return true;
}

static bool
json2value(json_t *object, struct rbh_value *value,
           struct rbh_sstack *sstack)
{
    if (json_is_integer(object)) {
        /* /!\ jansson stores integers as long long (i.e. int64_t) so we cannot
         * store uint64_t values in the xattrs.
         */
        value->type = RBH_VT_INT64;
        value->int64 = json_integer_value(object);
    } else if (json_is_object(object)) {
        value->type = RBH_VT_MAP;
        if (!json2value_map(object, &value->map, sstack))
            return false;
    } else if (json_is_array(object)) {
        value->type = RBH_VT_SEQUENCE;
        if (!json_array2sequence(object, value, sstack))
            return false;
    } else if (json_is_boolean(object)) {
        value->type = RBH_VT_BOOLEAN;
        value->boolean = json_boolean_value(object);
    } else if (json_is_string(object)) {
        const char *s = json_string_value(object);

        value->type = RBH_VT_STRING;
        value->string = rbh_sstack_alloc(sstack, s, strlen(s) + 1);
        if (!value->string)
            return false;
    } else if (json_is_null(object)) {
        value->type = RBH_VT_NULL;
    }

    return true;
}

bool
json2value_map(json_t *object, struct rbh_value_map *map,
               struct rbh_sstack *sstack)
{
    struct rbh_value_pair *pairs;
    const char *key;
    json_t *value;
    size_t count;
    size_t i = 0;

    count = json_object_size(object);
    pairs = rbh_sstack_alloc(sstack, NULL, sizeof(*pairs) * count);
    if (!pairs)
        return false;

    json_object_foreach(object, key, value) {
        struct rbh_value_pair *xattr = &pairs[i];
        struct rbh_value *tmp;

        tmp = rbh_sstack_alloc(sstack, NULL, sizeof(*xattr->value));
        if (!tmp)
            return false;

        if (!json2value(value, tmp, sstack))
            return false;

        xattr->key = rbh_sstack_alloc(sstack, key, strlen(key) + 1);
        if (!xattr->key)
            return false;

        xattr->value = tmp;
        i++;
    }

    map->pairs = pairs;
    map->count = count;
    return true;
}


static json_t *
value2json(const struct rbh_value *value, struct rbh_sstack *sstack)
{
    json_t *object = NULL;

    if (!value)
        return json_null();

    switch (value->type) {
    case RBH_VT_BOOLEAN:
        object = json_boolean(value->boolean);
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
        object = binary2json(value, sstack);
        break;
    case RBH_VT_REGEX:
        abort(); /* Regex values are not supposed to be stored in the backend */
        break;
    case RBH_VT_SEQUENCE:
        object = sequence2json(value, sstack);
        break;
    case RBH_VT_MAP:
        object = map2json(&value->map, sstack);
        break;
    case RBH_VT_NULL:
        object = json_null();
        break;
    }

    return object;
}

static json_t *
sequence2json(const struct rbh_value *sequence, struct rbh_sstack *sstack)
{
    json_t *array = json_array();

    for (size_t i = 0; i < sequence->sequence.count; i++) {
        json_t *elem;

        elem = value2json(&sequence->sequence.values[i], sstack);
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
binary2json(const struct rbh_value *binary, struct rbh_sstack *sstack)
{
    char *buf;

    buf = rbh_sstack_alloc(sstack, NULL, 2 * binary->binary.size + 1);
    if (!buf)
        return NULL;

    buf[2 * binary->binary.size] = '\0';
    bin2hex(binary, buf);

    return json_string(buf);
}

static json_t *
create_subobjects(json_t *parent, const char *key, struct rbh_sstack *sstack)
{
    json_t *subobject;
    char *subkey;
    char *dot;

    dot = strchr(key, '.');
    if (!dot)
        return parent;

    subkey = rbh_sstack_alloc(sstack, key, dot - key + 1);
    if (!subkey)
        return NULL;

    subkey[dot - key] = '\0';
    subobject = json_object_get(parent, subkey);
    if (!subobject) {
        subobject = json_object();
        if (!subobject)
            return NULL;
    }

    json_object_set(parent, subkey, subobject);

    return create_subobjects(subobject, dot + 1, sstack);
}

static const char *
last_key(const char *key)
{
    char *last_dot = strrchr(key, '.');

    if (last_dot)
        return last_dot + 1;

    return key;
}

static bool
set_xattr_value(json_t *object, const char *xattr,
                const struct rbh_value *value, struct rbh_sstack *sstack)
{
    json_t *json_value;
    json_t *subobject;

    json_value = value2json(value, sstack);
    if (!json_value)
        return false;

    subobject = create_subobjects(object, xattr, sstack);
    if (!subobject) {
        json_decref(json_value);
        return false;
    }

    json_object_set(subobject, last_key(xattr), json_value);
    json_decref(json_value);

    return true;
}

/**
 *  xattrs = { "xattr1" : { "op" : value }, "xattr2": { "op": value }}
 *
 *  Only one exception with nb_children:
 *
 *  xattrs = { "nb_children" :
 *              { "op" :
 *                  { "value": 1, "timestamp": xxx, "final": true }
 *              }
 *           }
 *
 *  But the timestamp and final value are not used as sqlite can't be used with
 *  MPI.
 */
json_t *
xattrs2json(const struct rbh_value_map *map, struct rbh_sstack *sstack)
{
    json_t *object;

    object = json_object();
    if (!object)
        return NULL;

    for (size_t i = 0; i < map->count; i++) {
        const struct rbh_value_map *operator_map = &map->pairs[i].value->map;
        const char *xattr = map->pairs[i].key;
        const char *operator;

        assert(operator_map->count == 1);

        operator = operator_map->pairs->key;
        // The increment operator are handle differently
        if (strcmp(operator, "inc") == 0)
            continue;

        if (!set_xattr_value(object, xattr, operator_map->pairs->value,
                             sstack)) {
            json_decref(object);
            return NULL;
        }
    }

    return object;
}

json_t *
map2json(const struct rbh_value_map *map, struct rbh_sstack *sstack)
{
    json_t *object;

    object = json_object();
    if (!object)
        return NULL;

    for (size_t i = 0; i < map->count; i++) {
        const char *xattr = map->pairs[i].key;

        if (!set_xattr_value(object, xattr, map->pairs[i].value, sstack)) {
            json_decref(object);
            return NULL;
        }
    }

    return object;
}
