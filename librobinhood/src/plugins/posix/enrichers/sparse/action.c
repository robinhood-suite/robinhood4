/* This file is part of RobinHood
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include <robinhood/backend.h>
#include <robinhood/fsentry.h>
#include <robinhood/projection.h>
#include <robinhood/utils.h>

#include "sparse_internals.h"

int
rbh_sparse_fill_entry_info(char *output, int max_length,
                           const struct rbh_fsentry *fsentry,
                           const char *directive, const char *backend)
{
    const struct rbh_value *value;

    (void) backend;

    assert(directive != NULL);
    assert(*directive != '\0');

    switch (*directive) {
    case 'S':
        value = rbh_fsentry_find_inode_xattr(fsentry, "sparseness");

        if (!value)
            return 0;

        return snprintf(output, max_length, "%.2f", value->uint32 / 100.0);
    }

    return 0;
}

int
rbh_sparse_fill_projection(struct rbh_filter_projection *projection,
                           const char *directive)
{
    assert(directive != NULL);
    assert(*directive != '\0');

    switch (*directive) {
    case 'S': // Sparseness
        rbh_projection_add(projection, str2filter_field("xattrs"));
        break;
    default:
        return 0;
    }

    return 1;
}
