/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include <robinhood/backend.h>
#include <robinhood/fsentry.h>
#include <robinhood/projection.h>
#include <robinhood/utils.h>

#include "acl_internals.h"

#define RBH_ACL_DIRECTIVE 'A'

static char *
perms2str(uint32_t perms, char string[4])
{
    string[0] = perms & 4 ? 'r' : '-';
    string[1] = perms & 2 ? 'w' : '-';
    string[2] = perms & 1 ? 'x' : '-';
    string[3] = '\0';

    return string;
}

static int
append_acl_entry(char *output, int max_length, int length,
                 const char *prefix, const char *type,
                 const char *identifier, uint32_t permissions)
{
    char perms[4];
    int rc;

    rc = snprintf(output + length, max_length - length,
                  "%s%s:%s:%s\n",
                  prefix, type, identifier,
                  perms2str(permissions, perms));
    if (rc < 0)
        return -1;

    return length + rc;
}

static int
print_named_entries(const struct rbh_value *entries,
                    const char *prefix, const char *type,
                    char *output, int max_length, int length)
{
    for (size_t i = 0; i < entries->sequence.count; i++) {
        const struct rbh_value *entry;
        const struct rbh_value *permissions;
        const struct rbh_value *id;
        char identifier[16];

        entry = &entries->sequence.values[i];
        assert(entry->type == RBH_VT_MAP);

        id = rbh_map_find(&entry->map, "id");
        permissions = rbh_map_find(&entry->map, "p");

        if (id == NULL || permissions == NULL)
            continue;

        snprintf(identifier, sizeof(identifier), "%u", id->uint32);

        length = append_acl_entry(output, max_length, length,
                                  prefix, type, identifier,
                                  permissions->uint32);
        if (length < 0)
            return -1;
    }

    return length;
}

static int
print_acl(const struct rbh_fsentry *fsentry, bool is_default,
          char *output, int max_length)
{
    const char *prefix = is_default ? "default:" : "";
    const struct rbh_value *entries;
    const struct rbh_value *value;
    const struct rbh_value *acl;
    int length = 0;
    uint32_t other;
    uint32_t group;
    uint32_t user;
    uint32_t mask;

    acl = rbh_fsentry_find_inode_xattr(fsentry,
            is_default ? "acl.default" : "acl.access");
    if (acl == NULL)
        return snprintf(output, max_length, "None");

    assert(acl->type == RBH_VT_MAP);

    value = rbh_map_find(&acl->map, "group");
    if (value == NULL)
        return snprintf(output, max_length, "None");
    group = value->uint32;

    if (is_default) {
        value = rbh_map_find(&acl->map, "owner");
        if (value == NULL)
            return snprintf(output, max_length, "None");
        user = value->uint32;

        value = rbh_map_find(&acl->map, "mask");
        if (value == NULL)
            return snprintf(output, max_length, "None");
        mask = value->uint32;

        value = rbh_map_find(&acl->map, "other");
        if (value == NULL)
            return snprintf(output, max_length, "None");
        other = value->uint32;
    } else {
        if (fsentry->statx == NULL)
            return snprintf(output, max_length, "None");

        user = (fsentry->statx->stx_mode >> 6) & 7;
        mask = (fsentry->statx->stx_mode >> 3) & 7;
        other = fsentry->statx->stx_mode & 7;
    }

    length = append_acl_entry(output, max_length, length,
                              prefix, "user", "", user);
    if (length < 0)
        return -1;

    entries = rbh_map_find(&acl->map, "users");
    if (entries != NULL) {
        length = print_named_entries(entries, prefix, "user",
                                     output, max_length, length);
        if (length < 0)
            return -1;
    }

    length = append_acl_entry(output, max_length, length,
                              prefix, "group", "", group);
    if (length < 0)
        return -1;

    entries = rbh_map_find(&acl->map, "groups");
    if (entries != NULL) {
        length = print_named_entries(entries, prefix, "group",
                                     output, max_length, length);
        if (length < 0)
            return -1;
    }

    length = append_acl_entry(output, max_length, length,
                              prefix, "mask", "", mask);
    if (length < 0)
        return -1;

    return append_acl_entry(output, max_length, length,
                            prefix, "other", "", other);
}

enum known_directive
rbh_acl_fill_entry_info(const struct rbh_fsentry *fsentry,
                        const char *format_string, size_t *index,
                        char *output, size_t *output_length, int max_length,
                        __attribute__((unused)) const char *backend)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;
    int tmp_length = 0;

    if (format_string[*index + 1] != RBH_NON_STANDARD_DIRECTIVE ||
        format_string[*index + 2] != RBH_ACL_DIRECTIVE)
        return RBH_DIRECTIVE_UNKNOWN;

    switch (format_string[*index + 3]) {
    case 'a':
        tmp_length = print_acl(fsentry, false, output, max_length);
        break;
    case 'd':
        tmp_length = print_acl(fsentry, true, output, max_length);
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
rbh_acl_fill_projection(struct rbh_filter_projection *projection,
                           const char *format_string, size_t *index)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;

    if (format_string[*index + 1] != RBH_NON_STANDARD_DIRECTIVE ||
        format_string[*index + 2] != RBH_ACL_DIRECTIVE)
        return RBH_DIRECTIVE_UNKNOWN;

    switch (format_string[*index + 3]) {
    case 'a':
        rbh_projection_add(projection,
                           str2filter_field("xattrs.acl.access"));
        rbh_projection_add(projection,
                           str2filter_field("statx.mode"));
    break;

    case 'd':
        rbh_projection_add(projection,
                           str2filter_field("xattrs.acl.default"));
        break;

    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        *index += 3;

    return rc;
}
