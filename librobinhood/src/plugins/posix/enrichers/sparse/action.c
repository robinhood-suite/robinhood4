/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
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
                           const char *format_string, size_t *index,
                           const char *backend)
{
    const struct rbh_value *value;

    (void) backend;

    assert(format_string != NULL);
    assert(format_string[*index] == '%');
    assert(format_string[*index + 1] != '\0');

    (*index)++;

    switch (format_string[*index]) {
    case 'S':
        value = rbh_fsentry_find_inode_xattr(fsentry, "sparseness");
        if (!value)
            return snprintf(output, max_length, "None");

        return snprintf(output, max_length, "%.2f", value->uint32 / 100.0);
    default:
        /* If we failed to identify the directive, let another plugin/extension
         * have a go at it
         */
        (*index)--;
    }

    return 0;
}

enum known_directive
rbh_sparse_fill_projection(struct rbh_filter_projection *projection,
                           const char *format_string, size_t *index)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;

    switch (format_string[*index + 1]) {
    case 'S': // Sparseness
        rbh_projection_add(projection, str2filter_field("xattrs.sparseness"));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        (*index)++;

    return rc;
}
