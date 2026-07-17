/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <endian.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/xattr.h>
#include <linux/posix_acl.h>
#include <linux/posix_acl_xattr.h>

#include "acl_internals.h"
#include "acl_xattr.h"

#include "robinhood/statx.h"
#include "robinhood/utils.h"
#include "robinhood/backends/common.h"
#include "robinhood/backends/acl.h"
#include "sstack.h"
#include "value.h"

#define ACL_ACCESS_XATTR  "system.posix_acl_access"
#define ACL_DEFAULT_XATTR "system.posix_acl_default"

#define ACL_KEY     "acl"
#define ACCESS_KEY  "access"
#define DEFAULT_KEY "default"

#define OWNER_KEY "owner"
#define GROUP_KEY "group"
#define MASK_KEY  "mask"
#define OTHER_KEY "other"

#define USERS_KEY  "users"
#define GROUPS_KEY "groups"

#define ID_KEY    "id"
#define PERMS_KEY "p"


static void *
get_acl_xattr(const struct entry_info *einfo, const char *name,
              size_t *size)
{
    ssize_t length;
    void *buffer;

    if (!einfo || !size)
        return NULL;

    if (einfo->inode_xattrs != NULL && einfo->inode_xattrs_count != NULL) {
        ssize_t count = *einfo->inode_xattrs_count;

        for (ssize_t i = 0; i < count; i++) {
            const struct rbh_value_pair *pair = &einfo->inode_xattrs[i];
            const struct rbh_value *value;

            if (strcmp(pair->key, name) != 0)
                continue;

            value = pair->value;
            if (!value || value->type != RBH_VT_BINARY) {
                errno = EINVAL;
                return NULL;
            }

            buffer = xmalloc(value->binary.size);
            memcpy(buffer, value->binary.data, value->binary.size);
            *size = value->binary.size;

            return buffer;
        }
    }

    if (!einfo->fd) {
        errno = ENODATA;
        return NULL;
    }

    length = fgetxattr(*einfo->fd, name, NULL, 0);
    if (length == -1)
        return NULL;

    buffer = xmalloc(length);

    length = fgetxattr(*einfo->fd, name, buffer, length);
    if (length == -1) {
        free(buffer);
        return NULL;
    }

    *size = length;
    return buffer;
}


static void
fill_uint32(struct rbh_value_pair *pair, const char *key,
            uint32_t number, struct rbh_sstack *values)
{
    struct rbh_value *value;

    value = RBH_SSTACK_PUSH(values, NULL, sizeof(*value));

    value->type = RBH_VT_UINT32;
    value->uint32 = number;

    pair->key = key;
    pair->value = value;
}


static void
fill_named_entry(struct rbh_value *value,
                 const struct acl_xattr_entry *entry,
                 struct rbh_sstack *values)
{
    struct rbh_value_pair *fields;

    fields = RBH_SSTACK_PUSH(values, NULL, 2 * sizeof(*fields));

    fill_uint32(&fields[0], ID_KEY, le32toh(entry->id), values);
    fill_uint32(&fields[1], PERMS_KEY, le16toh(entry->perms), values);

    value->type = RBH_VT_MAP;
    value->map.count = 2;
    value->map.pairs = fields;
}


static void
fill_named_entries(struct rbh_value_pair *pair, const char *key,
                   const struct acl_xattr *acl, uint16_t wanted_tag,
                   size_t count, struct rbh_sstack *values)
{
    struct rbh_value *sequence;
    struct rbh_value *items;
    size_t index = 0;

    sequence = RBH_SSTACK_PUSH(values, NULL, sizeof(*sequence));
    items = RBH_SSTACK_PUSH(values, NULL, count * sizeof(*items));

    pair->key = key;
    pair->value = sequence;

    sequence->type = RBH_VT_SEQUENCE;
    sequence->sequence.count = count;
    sequence->sequence.values = items;

    for (size_t i = 0; i < acl->count; i++) {
        const struct acl_xattr_entry *entry = &acl->entries[i];

        if (le16toh(entry->tag) != wanted_tag)
            continue;

        fill_named_entry(&items[index++], entry, values);
    }
}


static void
fill_access_acl(struct rbh_value_pair *pair, const struct acl_xattr *acl,
                struct rbh_sstack *values)
{
    struct rbh_value_pair *fields;
    struct rbh_value *map;
    size_t count;
    size_t index = 0;

    count = 1 + (acl->users_count != 0) + (acl->groups_count != 0);

    map = RBH_SSTACK_PUSH(values, NULL, sizeof(*map));
    fields = RBH_SSTACK_PUSH(values, NULL, count * sizeof(*fields));

    pair->key = ACCESS_KEY;
    pair->value = map;

    fill_uint32(&fields[index++], GROUP_KEY, acl->group, values);

    if (acl->users_count)
        fill_named_entries(&fields[index++], USERS_KEY, acl,
                           ACL_USER, acl->users_count, values);

    if (acl->groups_count)
        fill_named_entries(&fields[index++], GROUPS_KEY, acl,
                           ACL_GROUP, acl->groups_count, values);

    map->type = RBH_VT_MAP;
    map->map.count = index;
    map->map.pairs = fields;
}


static void
fill_default_acl(struct rbh_value_pair *pair, const struct acl_xattr *acl,
                 struct rbh_sstack *values)
{
    struct rbh_value_pair *fields;
    struct rbh_value *map;
    size_t count;
    size_t index = 0;

    count = 3 + acl->has_mask + (acl->users_count != 0) +
            (acl->groups_count != 0);

    map = RBH_SSTACK_PUSH(values, NULL, sizeof(*map));
    fields = RBH_SSTACK_PUSH(values, NULL, count * sizeof(*fields));

    pair->key = DEFAULT_KEY;
    pair->value = map;

    fill_uint32(&fields[index++], OWNER_KEY, acl->owner, values);
    fill_uint32(&fields[index++], GROUP_KEY, acl->group, values);

    if (acl->has_mask)
        fill_uint32(&fields[index++], MASK_KEY, acl->mask, values);

    fill_uint32(&fields[index++], OTHER_KEY, acl->other, values);

    if (acl->users_count)
        fill_named_entries(&fields[index++], USERS_KEY, acl,
                           ACL_USER, acl->users_count, values);

    if (acl->groups_count)
        fill_named_entries(&fields[index++], GROUPS_KEY, acl,
                           ACL_GROUP, acl->groups_count, values);

    map->type = RBH_VT_MAP;
    map->map.count = index;
    map->map.pairs = fields;
}


static void
fill_acl(struct rbh_value_pair *pair,
         const struct acl_xattr *access_acl, bool has_access,
         const struct acl_xattr *default_acl, bool has_default,
         struct rbh_sstack *values)
{
    struct rbh_value_pair *fields;
    struct rbh_value *map;
    size_t index = 0;

    map = RBH_SSTACK_PUSH(values, NULL, sizeof(*map));

    fields = RBH_SSTACK_PUSH(values, NULL,
                             (has_access + has_default) *
                             sizeof(*fields));

    pair->key = ACL_KEY;
    pair->value = map;

    if (has_access)
        fill_access_acl(&fields[index++], access_acl, values);

    if (has_default)
        fill_default_acl(&fields[index++], default_acl, values);

    map->type = RBH_VT_MAP;
    map->map.count = index;
    map->map.pairs = fields;
}


int
rbh_acl_enrich(struct entry_info *einfo, uint64_t flags,
               struct rbh_value_pair *pairs, size_t pairs_count,
               struct rbh_sstack *values)
{
    struct acl_xattr default_acl;
    struct acl_xattr access_acl;
    void *default_buffer = NULL;
    void *access_buffer = NULL;
    bool has_default = false;
    bool has_access = false;
    size_t default_size;
    size_t access_size;
    int rc = -1;

    if (!rbh_attr_is_acl(flags))
        return 0;

    access_buffer = get_acl_xattr(einfo, ACL_ACCESS_XATTR, &access_size);
    if (access_buffer) {
        if (acl_xattr_parse(access_buffer, access_size, &access_acl))
            goto out;
        has_access = true;
    }

    default_buffer = get_acl_xattr(einfo, ACL_DEFAULT_XATTR, &default_size);
    if (default_buffer) {
        if (acl_xattr_parse(default_buffer, default_size, &default_acl))
            goto out;
        has_default = true;
    }

    if (!has_access && !has_default) {
        rc = 0;
        goto out;
    }

    fill_acl(&pairs[0], &access_acl, has_access,
             &default_acl, has_default, values);

    rc = 1;

out:
    free(access_buffer);
    free(default_buffer);
    return rc;
}

int
rbh_acl_setup(void)
{
    return 0;
}
