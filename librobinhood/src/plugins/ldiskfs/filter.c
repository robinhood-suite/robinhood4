/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static void *
ldiskfs_iter_next(void *iterator)
{
    errno = ENODATA;
    return NULL;
}

static void
ldiskfs_iter_destroy(void *iterator)
{
    struct ldiskfs_iter *iter = iterator;

    free(iter);
}

static const struct rbh_mut_iterator_operations LDISKFS_ITER_OPS = {
    .next    = ldiskfs_iter_next,
    .destroy = ldiskfs_iter_destroy,
};

static const struct rbh_mut_iterator LDISKFS_ITER = {
    .ops = &LDISKFS_ITER_OPS,
};

static struct ldiskfs_iter *
ldiskfs_iter_new(struct ldiskfs_backend *ldiskfs)
{
    struct ldiskfs_iter *iter;
    int save_errno;

    iter = malloc(sizeof(*iter));
    if (!iter)
        return NULL;

    iter->iter = LDISKFS_ITER;
    iter->mdt_index = get_mdt_index(ldiskfs->fs);
    if (iter->mdt_index == -1)
        goto free_iter;

    iter->root = rbh_dcache_lookup(ldiskfs->dcache, EXT2_ROOT_INO, "ROOT");
    if (iter->root && !LINUX_S_ISDIR(iter->root->inode->i_mode)) {
        rbh_backend_error_printf("'ROOT' found, but is not a directory. Is this an MDT target?");
        goto free_iter;
    }
    if (iter->mdt_index == 0 && !iter->root) {
        rbh_backend_error_printf("MDT0000 must have the 'ROOT' directory");
        return NULL;
    }

    iter->remote_parent_dir = rbh_dcache_lookup(ldiskfs->dcache,
                                                EXT2_ROOT_INO,
                                                "REMOTE_PARENT_DIR");
    if (!iter->remote_parent_dir) {
        rbh_backend_error_printf("'REMOTE_PARENT_DIR' not found. Is this an MDT target?");
        goto free_iter;
    }

    if (!iter->remote_parent_dir) {
        rbh_backend_error_printf("'REMOTE_PARENT_DIR' found but is not a directory. Is this an MDT target?");
        goto free_iter;
    }

    return iter;

free_iter:
    save_errno = errno;
    free(iter);
    errno = save_errno;

    return NULL;
}

struct rbh_mut_iterator *
ldiskfs_backend_filter(void *backend, const struct rbh_filter *filter,
                       const struct rbh_filter_options *options,
                       const struct rbh_filter_output *output)
{
    struct ldiskfs_backend *ldiskfs = backend;
    struct ldiskfs_iter *iter;

    if (!scan_target(ldiskfs))
        return NULL;

    iter = ldiskfs_iter_new(ldiskfs);
    if (!iter)
        return NULL;

    return &iter->iter;
}
