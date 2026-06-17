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

#include "selinux_internals.h"

#define RBH_SELINUX_DIRECTIVE 'Z'

static int
print_selinux(const struct rbh_fsentry *fsentry, const char *xattr,
              char *output, int max_length)
{
    const struct rbh_value *value;

    value = rbh_fsentry_find_inode_xattr(fsentry, xattr);
    if (value == NULL)
        return snprintf(output, max_length, "None");

    return snprintf(output, max_length,"%s", value->string);
}

enum known_directive
rbh_selinux_fill_entry_info(const struct rbh_fsentry *fsentry,
                           const char *format_string, size_t *index,
                           char *output, size_t *output_length, int max_length,
                           __attribute__((unused)) const char *backend)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;
    int tmp_length = 0;

    if (format_string[*index + 1] != RBH_NON_STANDARD_DIRECTIVE ||
        format_string[*index + 2] != RBH_SELINUX_DIRECTIVE)
        return RBH_DIRECTIVE_UNKNOWN;

    switch (format_string[*index + 3]) {
    case 'c':
        tmp_length = print_selinux(fsentry, "selinux.context",
                                         output, max_length);
        break;
    case 'u':
        tmp_length = print_selinux(fsentry, "selinux.user",
                                         output, max_length);
        break;
    case 'r':
        tmp_length = print_selinux(fsentry, "selinux.role",
                                         output, max_length);
        break;
    case 't':
        tmp_length = print_selinux(fsentry, "selinux.type",
                                         output, max_length);
        break;
    case 'R':
        tmp_length = print_selinux(fsentry, "selinux.range",
                                         output, max_length);
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (tmp_length < 0)
        return RBH_DIRECTIVE_ERROR;

    if (rc == RBH_DIRECTIVE_KNOWN) {
        *output_length += tmp_length;
        *index += 3;
    }

    return rc;
}

enum known_directive
rbh_selinux_fill_projection(struct rbh_filter_projection *projection,
                           const char *format_string, size_t *index)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;

    if (format_string[*index + 1] != RBH_NON_STANDARD_DIRECTIVE ||
        format_string[*index + 2] != RBH_SELINUX_DIRECTIVE)
        return RBH_DIRECTIVE_UNKNOWN;

    switch (format_string[*index + 3]) {
    case 'c':
        rbh_projection_add(projection,
                           str2filter_field("xattrs.selinux.context"));
        break;
    case 'u':
        rbh_projection_add(projection,
                           str2filter_field("xattrs.selinux.user"));
        break;
    case 'r':
        rbh_projection_add(projection,
                           str2filter_field("xattrs.selinux.role"));
        break;
    case 't':
        rbh_projection_add(projection,
                           str2filter_field("xattrs.selinux.type"));
        break;
    case 'R':
        rbh_projection_add(projection,
                           str2filter_field("xattrs.selinux.range"));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        *index += 3;

    return rc;
}
