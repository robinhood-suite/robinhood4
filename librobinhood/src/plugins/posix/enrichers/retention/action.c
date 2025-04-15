/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include <robinhood/backend.h>
#include <robinhood/fsentry.h>
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
                              const char *directive, const char *backend)
{
    assert(directive != NULL);
    assert(*directive != '\0');

    switch (*directive) {
    case 'e':
        return write_expires_from_entry(fsentry, output, max_length);
    case 'E':
        return write_expiration_date_from_entry(fsentry, output, max_length);
    }

    return 0;
}
