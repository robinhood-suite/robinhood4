/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

#include <robinhood/statx.h>

static bool
is_dir(struct rbh_dentry *dentry)
{
    return LINUX_S_ISDIR(dentry->inode->i_mode);
}

#define ext2fs_inode_includes(size, field)              \
    ((size) >= (sizeof(((struct ext2_inode_large *)0)->field) + \
            offsetof(struct ext2_inode_large, field)))

static inline time_t decode_extra_sec(time_t seconds,
                    __u32 extra __attribute__((unused)))
{
#if (SIZEOF_TIME_T > 4)
    if (extra & EXT4_EPOCH_MASK)
        seconds += ((time_t)(extra & EXT4_EPOCH_MASK) << 32);
#endif
    return seconds;
}

#define ext2fs_inode_actual_size(inode)                       \
    ((size_t)(EXT2_GOOD_OLD_INODE_SIZE +                      \
          (sizeof(*inode) > EXT2_GOOD_OLD_INODE_SIZE ?            \
           ((struct ext2_inode_large *)(inode))->i_extra_isize : 0)))

#ifndef ext2fs_inode_xtime_get
#define ext2fs_inode_xtime_get(inode, field)                                         \
    (ext2fs_inode_includes(ext2fs_inode_actual_size(inode), field ## _extra) ?       \
         decode_extra_sec((inode)->field,                                          \
                            ((struct ext2_inode_large *)(inode))->field ## _extra) : \
        (time_t)(inode)->field)
#endif

#define inode_blocks(inode) \
    ((unsigned long long)inode->osd2.linux2.l_i_blocks_hi << 32) | \
    inode->i_blocks

static void
fifo_push(struct ldiskfs_iter *iter, struct rbh_dentry *dentry)
{
    g_queue_push_tail(iter->tasks, dentry);
}

static struct rbh_dentry *
fifo_pop(struct ldiskfs_iter *iter)
{
    return g_queue_pop_tail(iter->tasks);
}

static void
fifo_push_child_entries(struct ldiskfs_iter *iter,
                        struct rbh_dentry *dentry)
{
    GList *elem = dentry->children;
    while (elem) {
        fifo_push(iter, elem->data);
        elem = elem->next;
    }
}

static const struct rbh_id ROOT_ID = {
    .data = NULL,
    .size = 0,
};

static ssize_t
build_path(struct rbh_dentry *dentry, struct rbh_dentry *root,
           char **path, size_t len)
{
    ssize_t offset;

    if (dentry->ino == EXT2_ROOT_INO || dentry == root) {
        len += 2; /* '/\0' */
        *path = malloc(len);
        if (!*path)
            return -1;

        (*path)[0] = '/';
        (*path)[1] = '\0';
        return 1;
    }

    if (!dentry->parent)
        return 0;

    len += dentry->namelen + (is_dir(dentry) ? 1 : 0);
    offset = build_path(dentry->parent, root, path, len);
    if (offset == -1)
        return -1;

    memcpy((*path) + offset, dentry->name, dentry->namelen);
    offset += dentry->namelen;
    if (is_dir(dentry))
        (*path)[offset++] = '/';
    (*path)[offset] = '\0';

    return offset;
}

static char *
dentry_path(struct rbh_dentry *dentry, struct rbh_dentry *root)
{
    ssize_t offset;
    char *path;

    offset = build_path(dentry, root, &path, 0);
    if (offset == -1)
        return NULL;

    return path;
}

static struct rbh_fsentry *
fsentry_from_dentry(struct rbh_dentry *dentry, struct rbh_dentry *root)
{
    struct ext2_inode_large *inode = (struct ext2_inode_large *)dentry->inode;
    struct rbh_value_map inode_xattrs = {0};
    struct rbh_value_map ns_xattrs = {0};
    const struct rbh_id *parent_id;
    struct rbh_value path_value = {
        .type = RBH_VT_STRING,
    };
    struct rbh_value_pair path = {
        .key = "path",
        .value = &path_value,
    };
    struct rbh_fsentry *fsentry;
    struct rbh_statx statx;
    struct rbh_id *id;
    int save_errno;

    id = rbh_id_from_lu_fid(&dentry->fid);
    parent_id = dentry->parent ?
        rbh_id_from_lu_fid(&dentry->parent->fid) :
        &ROOT_ID;

    statx.stx_mask = RBH_STATX_ATIME_SEC | RBH_STATX_CTIME_SEC |
        RBH_STATX_MTIME_SEC | RBH_STATX_INO | RBH_STATX_BLOCKS |
        RBH_STATX_SIZE | RBH_STATX_MODE | RBH_STATX_UID | RBH_STATX_GID;
    /* statx.stx_blksize; */
    /* statx.stx_attributes; */
    statx.stx_nlink = inode->i_links_count;
    statx.stx_uid = inode_uid(*inode);
    statx.stx_gid = inode_gid(*inode);
    statx.stx_mode = inode->i_mode;
    statx.stx_ino = dentry->ino;
    statx.stx_size = inode->i_size;
    statx.stx_blocks = inode_blocks(inode);
    /* statx.stx_attributes_mask; */
    statx.stx_atime.tv_sec = ext2fs_inode_xtime_get(inode, i_atime);
    statx.stx_atime.tv_nsec = 0;
    statx.stx_mtime.tv_sec = ext2fs_inode_xtime_get(inode, i_mtime);
    statx.stx_mtime.tv_nsec = 0;
    statx.stx_ctime.tv_sec = ext2fs_inode_xtime_get(inode, i_ctime);
    statx.stx_ctime.tv_nsec = 0;
    statx.stx_btime.tv_sec = 0;
    statx.stx_btime.tv_nsec = 0;
    /* statx.stx_rdev_major; */
    /* statx.stx_rdev_minor; */
    /* statx.stx_dev_major; */
    /* statx.stx_dev_minor; */
    /* statx.stx_mnt_id; */

    ns_xattrs.count = 1;
    ns_xattrs.pairs = &path;
    path_value.string = dentry_path(dentry, root);

    fsentry = rbh_fsentry_new(id, parent_id, dentry->name,  &statx, &ns_xattrs,
                              &inode_xattrs, NULL);
    save_errno = errno;
    free((char *)path_value.string);
    errno = save_errno;

    return fsentry;
}

static void *
ldiskfs_iter_next(void *iterator)
{
    struct ldiskfs_iter *iter = iterator;
    struct rbh_dentry *dentry;

    dentry = fifo_pop(iter);
    if (!dentry) {
        errno = ENODATA;
        return NULL;
    }

    if (is_dir(dentry))
        fifo_push_child_entries(iter, dentry);

    return fsentry_from_dentry(dentry, iter->root);
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

    iter->tasks = g_queue_new();
    fifo_push_child_entries(iter, iter->remote_parent_dir);
    fifo_push(iter, iter->root);

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
