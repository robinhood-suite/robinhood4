/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "robinhood/utils.h"
#include "internals.h"

const char *
sqlite_xattr2json(const struct rbh_value_map *xattrs,
                  struct rbh_sstack *sstack)
{
    json_t *object;
    int save_errno;
    char *repr;
    char *dup;

    object = map2json(xattrs, sstack);
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
