/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static bool
json2value(json_t *object, struct rbh_value *value,
           struct rbh_sstack *sstack);

static bool
json_array2sequence(json_t *object, struct rbh_value *value,
                    struct rbh_sstack *sstack)
{
    struct rbh_value *values;
    json_t *elem;
    size_t index;

    value->sequence.count = json_array_size(object);
    values = rbh_sstack_alloc(sstack, sizeof(*values) * value->sequence.count);
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
        value->type = RBH_VT_STRING;
        value->string = rbh_sstack_strdup(sstack, json_string_value(object));
        if (!value->string)
            return false;
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
    pairs = rbh_sstack_alloc(sstack, sizeof(*pairs) * count);
    if (!pairs)
        return false;

    json_object_foreach(object, key, value) {
        struct rbh_value_pair *xattr = &pairs[i];
        struct rbh_value *tmp;

        tmp = rbh_sstack_alloc(sstack, sizeof(*xattr->value));
        if (!tmp)
            return false;

        if (!json2value(value, tmp, sstack))
            return false;

        xattr->key = rbh_sstack_strdup(sstack, key);
        if (!xattr->key)
            return false;

        xattr->value = tmp;
        i++;
    }

    map->pairs = pairs;
    map->count = count;
    return true;
}

bool
sqlite_json2xattrs(const char *json, struct rbh_value_map *xattrs,
                   struct rbh_sstack *sstack)
{
    json_error_t error;
    json_t *object;
    bool res;

    if (!json) {
        memset(xattrs, 0, sizeof(*xattrs));
        return true;
    }

    object = json_loads(json, 0, &error);
    if (!object)
        return sqlite_fail("failed to parse json xattrs '%s': %s",
                           json, error.text);

    res = json2value_map(object, xattrs, sstack);
    json_decref(object);
    return res;
}
