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

#include "retention_internals.h"

static int
write_expiration_date_from_entry(const struct rbh_fsentry *fsentry,
                                 char *output, int max_length)
{
    const struct rbh_value *value;

    if ((fsentry->mask & RBH_FP_INODE_XATTRS) == 0)
        return snprintf(output, max_length, "None");

    value = rbh_fsentry_find_inode_xattr(fsentry, "trusted.expiration_date");

    if (value == NULL || value->type != RBH_VT_INT64)
        return snprintf(output, max_length, "None");
    else if (value->int64 == INT64_MAX)
        return snprintf(output, max_length, "Inf");
    else
        return snprintf(output, max_length, "%s",
                        time_from_timestamp(&value->int64));
}

static int
write_expires_from_entry(const struct rbh_fsentry *fsentry,
                         char *output, int max_length)
{
    static const char *retention_attribute;
    const struct rbh_value *value;

    if (retention_attribute == NULL)
        retention_attribute = rbh_config_get_string(XATTR_EXPIRES_KEY,
                                                    "user.expires");

    if (retention_attribute == NULL)
        return snprintf(output, max_length, "None");
    if ((fsentry->mask & RBH_FP_INODE_XATTRS) == 0)
        return snprintf(output, max_length, "None");

    value = rbh_fsentry_find_inode_xattr(fsentry, retention_attribute);

    if (value == NULL || value->type != RBH_VT_STRING)
        return snprintf(output, max_length, "None");
    else
        return snprintf(output, max_length, "%s", value->string);
}

int
rbh_retention_fill_entry_info(char *output, int max_length,
                              const struct rbh_fsentry *fsentry,
                              const char *format_string, size_t *index,
                              const char *backend)
{
    assert(format_string != NULL);
    assert(format_string[*index] == '%');
    assert(format_string[*index + 1] != '\0');

    (*index)++;

    switch (format_string[*index]) {
    case 'e':
        return write_expires_from_entry(fsentry, output, max_length);
    case 'E':
        return write_expiration_date_from_entry(fsentry, output, max_length);
    default:
        /* If we failed to identify the directive, let another plugin/extension
         * have a go at it
         */
        (*index)--;
    }

    return 0;
}

enum known_directive
rbh_retention_fill_projection(struct rbh_filter_projection *projection,
                              const char *format_string, size_t *index)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;
    const char *retention_attribute;
    char xattr[1024];

    switch (format_string[*index + 1]) {
    case 'e':
        retention_attribute = rbh_config_get_string(XATTR_EXPIRES_KEY,
                                                    "user.expires");

        if (sprintf(xattr, "xattrs.%s", retention_attribute) <= 0) {
            rc = RBH_DIRECTIVE_ERROR;
            break;
        }

        rbh_projection_add(projection, str2filter_field(xattr));
        break;
    case 'E':
        rbh_projection_add(projection,
                           str2filter_field("xattrs.trusted.expiration_date"));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        (*index)++;

    return rc;
}
