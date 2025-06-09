/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "dcache.h"
#include <robinhood/utils.h>

#include <assert.h>

struct rbh_dentry *
rbh_dentry_new(ext2_ino_t ino)
{
    struct rbh_dentry *dentry;

    dentry = calloc(sizeof(*dentry), 1);
    if (!dentry)
        return NULL;

    dentry->ino = ino;
    return dentry;
}
struct rbh_dcache *rbh_dcache_new(void)
{
    struct rbh_dcache *dcache;
    int save_errno;

    dcache = malloc(sizeof(*dcache));
    if (!dcache)
        return NULL;

    dcache->dentries = g_hash_table_new(g_int_hash, g_int_equal);
    if (!dcache->dentries) {
        save_errno = errno;
        free(dcache);
        errno = save_errno;
        return NULL;
    }

    return dcache;
}

void rbh_dcache_destroy(struct rbh_dcache *dcache)
{
    g_hash_table_destroy(dcache->dentries);
    free(dcache);
}

struct rbh_dentry *
rbh_dcache_find(struct rbh_dcache *dcache, ext2_ino_t ino)
{
    return g_hash_table_lookup(dcache->dentries, &ino);
}

struct rbh_dentry *
rbh_dcache_find_or_create(struct rbh_dcache *dcache, ext2_ino_t ino)
{
    struct rbh_dentry *dentry;

    dentry = g_hash_table_lookup(dcache->dentries, &ino);
    if (!dentry) {
        dentry = rbh_dentry_new(ino);
        if (!dentry)
            return NULL;

        g_hash_table_insert(dcache->dentries, &dentry->ino, dentry);
    }

    assert(dentry->ino == ino);

    return dentry;
}

static gint
glist_name_cmp(gconstpointer a, gconstpointer b)
{
    const struct rbh_dentry *dentry = a;
    const char *name = b;

    return strncmp(dentry->name, name, dentry->namelen);
}

static struct rbh_dentry *
rbh_dir_lookup(struct rbh_dentry *parent, const char *name)
{
    GList *child;

    if (!LINUX_S_ISDIR(parent->inode->i_mode)) {
        errno = ENOTDIR;
        return NULL;
    }

    child = g_list_find_custom(parent->children, name, glist_name_cmp);
    if (!child) {
        errno = ENOENT;
        return NULL;
    }

    return child->data;
}

struct rbh_dentry *
rbh_dcache_lookup(struct rbh_dcache *dcache, ext2_ino_t ino, const char *name)
{
    struct rbh_dentry *dentry;

    dentry = rbh_dcache_find(dcache, ino);
    if (!dentry) {
        errno = ENOENT;
        return NULL;
    }

    return rbh_dir_lookup(dentry, name);
}

struct iter_data {
    rbh_dcache_cb_t cb;
    void *udata;
};

static void glib_foreach_cb(gpointer ino, gpointer dentry, gpointer udata)
{
    struct iter_data *data = udata;

    (void) ino;

    data->cb(dentry, data->udata);
}

void
rbh_dcache_foreach(struct rbh_dcache *dcache, rbh_dcache_cb_t cb, void *udata)
{
    struct iter_data data = {
        .udata = udata,
        .cb = cb,
    };
    g_hash_table_foreach(dcache->dentries, glib_foreach_cb, &data);
}
