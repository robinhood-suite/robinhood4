/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "robinhood/utils.h"
#include "internals.h"

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
