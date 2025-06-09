/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

bool
sqlite_json2xattrs(const char *json, struct rbh_value_map *xattrs)
{
    memset(xattrs, 0, sizeof(*xattrs));
    return true;
}
