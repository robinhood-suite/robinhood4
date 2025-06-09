/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "dcache.h"

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

static void *
rbh_dcache_iter_next(void *iterator)
{
    errno = ENODATA;
    return NULL;
}

static void
rbh_dcache_iter_destroy(void *iterator)
{
    struct rbh_dcache_iter *iter = iterator;

    free(iter);
}

static const struct rbh_mut_iterator_operations DCACHE_ITER_OPS = {
    .next    = rbh_dcache_iter_next,
    .destroy = rbh_dcache_iter_destroy,
};

static const struct rbh_mut_iterator DCACHE_ITER = {
    .ops = &DCACHE_ITER_OPS,
};

struct rbh_dcache_iter *
rbh_dcache_iter_new(struct rbh_dcache *dcache)
{
    struct rbh_dcache_iter *iter;

    iter = xmalloc(sizeof(*iter));
    iter->iter = DCACHE_ITER;

    return iter;
}
