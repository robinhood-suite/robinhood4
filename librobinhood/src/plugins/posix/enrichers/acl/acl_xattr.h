/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_POSIX_ACL_XATTR_H
#define RBH_POSIX_ACL_XATTR_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct rbh_value;

struct acl_xattr_entry {
    uint16_t tag;
    uint16_t perms;
    uint32_t id;
};

struct acl_xattr {
    const struct acl_xattr_entry *entries;
    size_t count;

    uint16_t owner;
    uint16_t group;
    uint16_t mask;
    uint16_t other;

    size_t users_count;
    size_t groups_count;

    bool has_mask;
};

int
acl_xattr_parse(const void *buffer, size_t size, struct acl_xattr *acl);

#endif
