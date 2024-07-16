/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/config.h"

#include "xattrs_mapping.h"

#define XATTRS_MAP_ADDRESS_KEY "xattrs_map"

static struct rbh_value_map *xattrs_types;

struct rbh_value *
create_value_from_xattr(const char *name, char *buffer, ssize_t length,
                        struct rbh_sstack *xattrs)
{
    struct rbh_value *value;

    value = rbh_sstack_push(xattrs, NULL, sizeof(*value));
    if (value == NULL)
        return NULL;

    value->binary.data = rbh_sstack_push(xattrs, buffer, length);
    if (value->binary.data == NULL)
        return NULL;

    value->binary.size = length;
    value->type = RBH_VT_BINARY;

    return value;
}

int
set_xattrs_types_map()
{
    struct rbh_value value = { 0 };
    enum key_parse_result rc;

    rc = rbh_config_find(XATTRS_MAP_ADDRESS_KEY, &value, RBH_VT_MAP);
    if (rc == KPR_ERROR)
        return -1;

    if (rc == KPR_NOT_FOUND)
        xattrs_types = NULL;

    return 0;
}
