/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "xattrs_mapping.h"

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
