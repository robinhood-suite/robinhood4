/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HEAVE_CONFIG_H
# include "config.h"
#endif

#include <endian.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/xattr.h>
#include <linux/posix_acl.h>
#include <linux/posix_acl_xattr.h>

#include "robinhood/statx.h"
#include "robinhood/utils.h"
#include "sstack.h"
#include "value.h"
#include "acl_xattr.h"


static int
invalid_acl(void)
{
    errno = EINVAL;
    return -1;
}

int
acl_xattr_parse(const void * buffer, size_t size, struct acl_xattr *acl)
{
    const struct posix_acl_xattr_header *header;
    const struct posix_acl_xattr_entry *entries;
    bool has_owner = false;
    bool has_group = false;
    bool has_other = false;
    size_t entries_size;

    if (!buffer || !acl)
        return invalid_acl();

    if (size < sizeof(*header))
        return invalid_acl();

    header = buffer;

    if (le32toh(header->a_version) != POSIX_ACL_XATTR_VERSION)
            return invalid_acl();

    entries_size = size - sizeof(*header);

    if (entries_size % sizeof(*entries))
        return invalid_acl();

    memset(acl, 0, sizeof(*acl));

    entries = (const void *)(header + 1);

    acl->entries = (const struct acl_xattr_entry *)entries;
    acl->count = entries_size / sizeof(*entries);

    for (size_t i = 0; i < acl->count; i++) {
        uint16_t perms = le16toh(entries[i].e_perm);
        uint16_t tag = le16toh(entries[i].e_tag);

        switch (tag) {
            case ACL_USER_OBJ:
                acl->owner = perms;
                has_owner = true;
                break;

            case ACL_USER:
                acl->users_count++;
                break;

            case ACL_GROUP_OBJ:
                acl->group = perms;
                has_group = true;
                break;

            case ACL_GROUP:
                acl->groups_count++;
                break;

            case ACL_MASK:
                acl->mask = perms;
                acl->has_mask = true;
                break;

            case ACL_OTHER:
                acl->other = perms;
                has_other = true;
                break;

            default:
                return invalid_acl();
        }
    }

    if (!has_owner || !has_group || !has_other)
        return invalid_acl();

    return 0;
}
