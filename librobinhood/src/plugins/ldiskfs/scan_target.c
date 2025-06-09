/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

#define ldiskfs_error(rc, fmt, ...) \
    ({                                                               \
        rbh_backend_error_printf("ldiskfs: " fmt ": %s"__VA_OPT__(,) \
                                 __VA_ARGS__, error_message(rc));    \
        false;                                                       \
    })

static struct ext2_inode *
dup_inode(struct ext2_inode *src, size_t inode_size)
{
    struct ext2_inode *dst;

    dst = malloc(inode_size);
    if (!dst)
        return NULL;

    memcpy(dst, src, inode_size);

    return dst;
}

static int
scan_dir_cb(ext2_filsys fs,
            blk64_t *block_nr,
            e2_blkcnt_t block_count,
            blk64_t ref_block,
            int ref_offset,
            void *udata)
{
    ext2_ino_t ino = *(ext2_ino_t *)udata;

    (void) ref_block;
    (void) ref_offset;

    if ((int) block_count < 0)
        return 0;

    if (ext2fs_add_dir_block2(fs->dblist, ino, *block_nr, block_count))
        return BLOCK_ABORT;

    return 0;
}

static bool
add_dir_blocks(struct ldiskfs_backend *ldiskfs,
               ext2_ino_t ino,
               struct ext2_inode *inode,
               size_t inode_size)
{
    struct rbh_dentry *dentry;
    errcode_t rc;
    char *buf;

    dentry = rbh_dcache_find_or_create(ldiskfs->dcache, ino);
    dentry->inode = dup_inode(inode, inode_size);
    if (!inode)
        return false;

    /* TODO this could be moved to the ldiskfs_backend struct to do a single
     * allocation
     */
    buf = malloc(ldiskfs->fs->blocksize * 3);
    if (!buf)
        return false;

    rc = ext2fs_block_iterate3(ldiskfs->fs, ino, 0, buf, scan_dir_cb, &ino);
    if (rc) {
        free(buf);
        return ldiskfs_error(rc,
                         "failed to iterate through directory blocks of '%d'",
                         ino);
    }

    free(buf);

    return true;
}


/* Scan all the inodes from all the groups and fetch inline xattrs (xattrs
 * stored alongside the inode). Inodes containing external xattrs are kept in
 * memory to read them later.
 */
static bool
scan_inodes(struct ldiskfs_backend *backend)
{
    struct rbh_dentry *dentry;
    struct ext2_inode *inode;
    size_t inode_size;
    ext2_ino_t ino;
    errcode_t rc;

    rc = ext2fs_read_inode_bitmap(backend->fs);
    if (rc)
        return ldiskfs_error(rc, "failed to read inode bitmap");

    rc = ext2fs_init_dblist(backend->fs, NULL);
    if (rc)
        return ldiskfs_error(rc, "failed to init directory block list");

    rc = ext2fs_open_inode_scan(backend->fs,
                                backend->fs->inode_blocks_per_group,
                                &backend->iscan);
    if (rc)
        return ldiskfs_error(rc, "failed to init inode scan");

    inode_size = EXT2_INODE_SIZE(backend->fs->super);
    inode = malloc(inode_size);
    if (!inode)
        return false;

    while (!ext2fs_get_next_inode_full(backend->iscan, &ino, inode,
                                       inode_size)) {
        if (ino == 0)
            break;

        if (ino < EXT2_GOOD_OLD_FIRST_INO && ino != EXT2_ROOT_INO)
            /* skip reserved inodes except the root */
            continue;

        if (!ext2fs_fast_test_inode_bitmap2(backend->fs->inode_map, ino))
            /* skip deleted inodes */
            continue;

        if (LINUX_S_ISDIR(inode->i_mode)) {
            if (!add_dir_blocks(backend, ino, inode, inode_size))
                return false;
        } else {
            dentry = rbh_dcache_find_or_create(backend->dcache, ino);
            dentry->inode = dup_inode(inode, inode_size);
            if (!inode)
                return false;
        }
    }

    free(inode);
    ext2fs_close_inode_scan(backend->iscan);

    return true;
}

static bool
scan_dentries(struct ldiskfs_backend *backend)
{
    return true;
}

bool
scan_target(struct ldiskfs_backend *backend)
{
    return scan_inodes(backend) &&
        scan_dentries(backend);
}
