/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_DCACHE_H
#define ROBINHOOD_DCACHE_H

#include <ext2fs/ext2fs.h>
#include <glib.h>

#include <robinhood/fsentry.h>
#include <robinhood/iterator.h>
#include <robinhood/utils.h>

struct rbh_dcache {
    GHashTable *dentries;
};

struct rbh_dentry_xattr {
    char *name;
    void *value;
    size_t value_len;
};

struct rbh_dentry {
    ext2_ino_t ino;
    struct ext2_inode *inode;
    const char *name;
    size_t namelen;
    struct rbh_dentry *parent;
    GList *children;
    GList *xattrs;
};

struct rbh_dentry *
rbh_dentry_new(ext2_ino_t ino);

struct rbh_dcache *
rbh_dcache_new(void);

void
rbh_dcache_destroy(struct rbh_dcache *dcache);

struct rbh_dentry *
rbh_dcache_find(struct rbh_dcache *dcache, ext2_ino_t ino);

struct rbh_dentry *
rbh_dcache_find_or_create(struct rbh_dcache *dcache, ext2_ino_t ino);

typedef void (*rbh_dcache_cb_t)(struct rbh_fsentry *fsentry, void *udata);

void
rbh_dcache_foreach(struct rbh_dcache *dcache, rbh_dcache_cb_t cb, void *udata);

struct rbh_dentry *
rbh_dcache_lookup(struct rbh_dcache *dcache, ext2_ino_t ino, const char *name);

#endif
