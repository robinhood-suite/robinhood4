/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

enum map2json_mode {
    MAP2JSON_DIRECT,   // map->pairs[i].value
    MAP2JSON_INODE_OP, // map->pairs[i].value->map.pairs[0].value
};

static json_t *
map2json(const struct rbh_value_map *value, struct rbh_sstack *sstack,
         enum map2json_mode mode);

static json_t *
sequence2json(const struct rbh_value *value, struct rbh_sstack *sstack);

static json_t *
binary2json(const struct rbh_value *value,
            struct rbh_sstack *sstack);

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
        object = map2json(&value->map, sstack, MAP2JSON_DIRECT);
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

static const struct rbh_value *
get_value_for_json(const struct rbh_value_map *map, size_t i,
                   enum map2json_mode mode)
{
    const struct rbh_value_map *op_map;

    if (mode == MAP2JSON_DIRECT)
        return map->pairs[i].value;

    op_map = &map->pairs[i].value->map;
    assert(op_map->count == 1);

    return op_map->pairs[0].value;
}

static json_t *
map2json(const struct rbh_value_map *map, struct rbh_sstack *sstack,
         enum map2json_mode mode)
{
    json_t *object;

    object = json_object();
    if (!object)
        return NULL;

    for (size_t i = 0; i < map->count; i++) {
        const char *xattr = map->pairs[i].key;
        const struct rbh_value *_value;
        json_t *subobject;
        json_t *value;

        // FIXME this is very ugly...
        if (!strcmp(xattr, "nb_children"))
            continue;

        _value = get_value_for_json(map, i, mode);

        value = value2json(_value, sstack);
        if (!value) {
            json_decref(object);
            return NULL;
        }

        subobject = create_subobjects(object, xattr, sstack);
        if (!subobject) {
            json_decref(object);
            return NULL;
        }

        json_object_set(subobject, last_key(xattr), value);
    }

    return object;
}

const char *
sqlite_xattr2json(const struct rbh_value_map *xattrs,
                  struct rbh_sstack *sstack, bool inode)
{
    json_t *object;
    int save_errno;
    char *repr;
    char *dup;

    object = map2json(xattrs, sstack,
                      inode ? MAP2JSON_INODE_OP : MAP2JSON_DIRECT);
    if (!object)
        return NULL;

    repr = json_dumps(object, JSON_COMPACT);
    json_decref(object);
    if (!repr)
        return NULL;

    dup = rbh_sstack_alloc(sstack, repr, strlen(repr) + 1);
    save_errno = errno;
    free(repr);
    errno = save_errno;
    return dup;
}

json_t *
sqlite_list2array(const char **values, size_t n_values)
{
    json_t *array = json_array();

    for (size_t i = 0; i < n_values; i++)
        json_array_append(array, json_string(values[i]));

    return array;
}
