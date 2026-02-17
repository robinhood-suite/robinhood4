/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <jansson.h>

#include "robinhood/utils.h"

bool
rbh_action_parameters_to_value_map(const char *parameters,
                                   struct rbh_value_map *map,
                                   struct rbh_sstack *sstack)
{
    json_error_t error;
    bool rc = false;
    json_t *root;

    if (!parameters || !map || !sstack)
        return false;

    root = json_loads(parameters, 0, &error);
    if (!root)
        return false;

    if (!json_is_object(root)) {
        json_decref(root);
        return false;
    }

    rc = json2value_map(root, map, sstack);
    json_decref(root);
    return rc;
}
