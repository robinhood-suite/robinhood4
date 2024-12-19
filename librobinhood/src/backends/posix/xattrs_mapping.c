/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>

#include "robinhood/config.h"
#include "robinhood/utils.h"

#include "xattrs_mapping.h"

#define XATTRS_MAP_ADDRESS_KEY "xattrs_map"

static struct rbh_value_map *xattrs_types;
static size_t xattrs_types_count = 1 << 7;
static struct rbh_sstack *xattrs_types_stack;

__attribute__((destructor))
void
free_xattrs_variables(void)
{
    if (xattrs_types)
        rbh_sstack_destroy(xattrs_types_stack);
}

static struct rbh_value *
set_value_to_binary(struct rbh_sstack *xattrs, const char *buffer,
                    ssize_t length, struct rbh_value *value)
{
    value->binary.data = RBH_SSTACK_PUSH(xattrs, buffer, length + 1);

    value->binary.size = length;
    value->type = RBH_VT_BINARY;

    return value;
}

static struct rbh_value *
set_value_to_boolean(const char *name, const char *buffer,
                     ssize_t length, struct rbh_value *value)
{
    if (strcmp(buffer, "true") == 0)
        value->boolean = true;
    else if (strcmp(buffer, "false") == 0)
        value->boolean = false;
    else
        goto err;

    value->type = RBH_VT_BOOLEAN;

    return value;

err:
    fprintf(stderr,
            "Unexpected value for boolean-type xattr '%s', found '%s'\n",
            name, buffer);
    errno = EINVAL;
    return NULL;
}

static struct rbh_value *
set_value_to_int(enum rbh_value_type type, const char *name, const char *buffer,
                 struct rbh_value *value)
{
    int64_t _value;
    int rc;

    rc = str2int64_t(buffer, &_value);
    if (rc) {
        fprintf(stderr,
                "Unexpected value for %s-type xattr '%s', found '%s'\n",
                value_type2str(type), name, buffer);
        return NULL;
    }

    if (type == RBH_VT_INT32)
        value->int32 = _value;
    else
        value->int64 = _value;

    value->type = type;

    return value;
}

static struct rbh_value *
set_value_to_uint(enum rbh_value_type type, const char *name,
                  const char *buffer, struct rbh_value *value)
{
    uint64_t _value;
    int rc;

    rc = str2uint64_t(buffer, &_value);
    if (rc) {
        fprintf(stderr,
                "Unexpected value for %s-type xattr '%s', found '%s'\n",
                value_type2str(type), name, buffer);
        return NULL;
    }

    if (type == RBH_VT_UINT32)
        value->uint32 = _value;
    else
        value->uint64 = _value;

    value->type = type;

    return value;
}

static struct rbh_value *
set_value_to_string(struct rbh_sstack *xattrs, const char *buffer,
                    ssize_t length, struct rbh_value *value)
{
    value->string = RBH_SSTACK_PUSH(xattrs, buffer, length + 1);
    value->type = RBH_VT_STRING;

    return value;
}


struct rbh_value *
create_value_from_xattr(const char *name, const char *buffer, ssize_t length,
                        struct rbh_sstack *xattrs)
{
    struct rbh_value *value;

    value = RBH_SSTACK_PUSH(xattrs, NULL, sizeof(*value));

    if (xattrs_types == NULL)
        return set_value_to_binary(xattrs, buffer, length, value);

    for (int i = 0; i < xattrs_types->count; i++) {
        if (name[0] != xattrs_types->pairs[i].key[0])
            continue;

        if (strcmp(&name[1], &xattrs_types->pairs[i].key[1]))
            continue;

        switch (xattrs_types->pairs[i].value->type) {
        case RBH_VT_BOOLEAN:
            return set_value_to_boolean(name, buffer, length, value);
        case RBH_VT_INT32:
        case RBH_VT_INT64:
            return set_value_to_int(xattrs_types->pairs[i].value->type,
                                    name, buffer, value);
        case RBH_VT_UINT32:
        case RBH_VT_UINT64:
            return set_value_to_uint(xattrs_types->pairs[i].value->type,
                                     name, buffer, value);
        case RBH_VT_STRING:
            return set_value_to_string(xattrs, buffer, length, value);
        default:
            return set_value_to_binary(xattrs, buffer, length, value);
        }
    }

    return set_value_to_binary(xattrs, buffer, length, value);
}

static int
extract_type_from_map(struct rbh_value_map map, struct rbh_value *value,
                      int index)
{
    enum rbh_value_type type;
    const char *typing;

    if (map.pairs[index].value->type != RBH_VT_STRING) {
        fprintf(stderr,
                "A value corresponding to a typing is not specified as a string in the configuration file (key is '%s')\n",
                map.pairs[index].key);
        return -1;
    }

    typing = map.pairs[index].value->string;
    type = str2value_type(typing);

    switch (type) {
    case RBH_VT_BOOLEAN:
    case RBH_VT_INT32:
    case RBH_VT_UINT32:
    case RBH_VT_INT64:
    case RBH_VT_UINT64:
    case RBH_VT_STRING:
    case RBH_VT_BINARY:
        value->type = type;
        break;
    case RBH_VT_SEQUENCE:
    case RBH_VT_REGEX:
    case RBH_VT_MAP:
        fprintf(stderr,
                "Typings 'regex', 'sequence' and 'map' not managed yet\n");
        errno = ENOTSUP;
        return -1;
    default:
        fprintf(stderr, "Invalid typing found '%s'\n", typing);
        errno = EINVAL;
        return -1;
    }

    return 0;
}

int
set_xattrs_types_map()
{
    struct rbh_value value = { 0 };
    struct rbh_value_pair *pairs;
    enum key_parse_result rc;

    rc = rbh_config_find(XATTRS_MAP_ADDRESS_KEY, &value, RBH_VT_MAP);
    if (rc == KPR_ERROR)
        return -1;

    if (rc == KPR_NOT_FOUND) {
        xattrs_types = NULL;
        return 0;
    }

    xattrs_types_stack = rbh_sstack_new(xattrs_types_count);
    if (xattrs_types_stack == NULL)
        return false;

    xattrs_types = RBH_SSTACK_PUSH(xattrs_types_stack, NULL,
                                   sizeof(*xattrs_types));

    pairs = RBH_SSTACK_PUSH(xattrs_types_stack, NULL,
                            value.map.count * sizeof(*pairs));

    for (int i = 0; i < value.map.count; i++) {
        struct rbh_value *_value;

        pairs[i].key = value.map.pairs[i].key;
        _value = RBH_SSTACK_PUSH(xattrs_types_stack, NULL, sizeof(*_value));

        if (extract_type_from_map(value.map, _value, i))
            return -1;

        pairs[i].value = _value;
    }

    xattrs_types->pairs = pairs;
    xattrs_types->count = value.map.count;

    return 0;
}
