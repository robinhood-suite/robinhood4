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

struct rbh_value *
create_value_from_xattr(const char *name, char *buffer, ssize_t length,
                        struct rbh_sstack *xattrs)
{
    struct rbh_value *value;

    value = rbh_sstack_push(xattrs, NULL, sizeof(*value));
    if (value == NULL)
        return NULL;

    value->binary.data = rbh_sstack_push(xattrs, buffer, length + 1);
    if (value->binary.data == NULL)
        return NULL;

    value->binary.size = length;
    value->type = RBH_VT_BINARY;

    return value;
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

    xattrs_types = rbh_sstack_push(xattrs_types_stack, NULL,
                                   sizeof(*xattrs_types));
    if (xattrs_types == NULL)
        return -1;

    pairs = rbh_sstack_push(xattrs_types_stack, NULL,
                            value.map.count * sizeof(*pairs));
    if (pairs == NULL)
        return -1;

    for (int i = 0; i < value.map.count; i++) {
        struct rbh_value *_value;

        pairs[i].key = value.map.pairs[i].key;
        _value = rbh_sstack_push(xattrs_types_stack, NULL, sizeof(*_value));
        if (_value == NULL)
            return -1;

        if (extract_type_from_map(value.map, _value, i))
            return -1;

        pairs[i].value = _value;
    }

    xattrs_types->pairs = pairs;
    xattrs_types->count = value.map.count;

    return 0;
}
