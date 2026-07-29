/* This file is part of RobinHood
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "internals.h"

static struct ext2_inode *
dup_inode(struct ext2_inode *src, size_t inode_size)
{
    struct ext2_inode *dst;

    dst = xmalloc(inode_size);
    memcpy(dst, src, inode_size);

    return dst;
}

struct db_data {
    ext2_ino_t ino;
    ext2_dblist dblist;
};

static int
scan_dir_cb(ext2_filsys fs,
            blk64_t *block_nr,
            e2_blkcnt_t block_count,
            blk64_t ref_block,
            int ref_offset,
            void *udata)
{
    struct db_data _udata = *(struct db_data *)udata;
    ext2_dblist dblist = _udata.dblist;
    ext2_ino_t ino = _udata.ino;

    (void) ref_block;
    (void) ref_offset;

    if ((int) block_count < 0)
        return 0;

    if (ext2fs_add_dir_block2(dblist, ino, *block_nr, block_count))
        return BLOCK_ABORT;

    return 0;
}

static bool
add_dir_blocks(struct ldiskfs_backend *ldiskfs,
               ext2_ino_t ino,
               struct ext2_inode *inode,
               size_t inode_size,
               struct rbh_dcache *dcache,
               ext2_dblist dblist)
{
    struct rbh_dentry *dentry;
    errcode_t rc;
    char *buf;

    dentry = rbh_dcache_find_or_create(dcache, ino);
    dentry->inode = dup_inode(inode, inode_size);

    /* TODO this could be moved to the ldiskfs_backend struct to do a single
     * allocation
     */
    /* ext2fs_block_iterate3 can read indirect, double-indirect and triple-indirect
     * blocks during the iteration over the directory's blocks. It therefore needs
     * 3 blocks of size blocksize.
     */
    buf = xmalloc(ldiskfs->fs->blocksize * 3);

    struct db_data udata = {
        .ino = ino,
        .dblist = dblist,
    };

    // XXX bad behaviour is probably caused by this line
    rc = ext2fs_block_iterate3(ldiskfs->fs, ino, 0, buf, scan_dir_cb, &udata);
    if (rc) {
        free(buf);
        return ldiskfs_error(
            "failed to iterate through directory blocks of '%d': %s",
            ino, error_message(rc)
        );
    }

    free(buf);

    return true;
}

void *scan_inodes_thr (void *arg)
{
    struct ldiskfs_backend *backend;
    struct rbh_dcache *dcache;
    struct rbh_dentry *dentry;
    struct ext2_inode *inode;
    ext2_inode_scan iscan;
    ext2_dblist dblist;
    size_t inode_size;
    ext2_ino_t ino;
    int num_groups;
    int curr_group;
    errcode_t rc;
    int thr_id;
    bool *ret;

    backend = (struct ldiskfs_backend *)arg;
    thr_id = backend->thread_counter++;
    ret = malloc(sizeof(bool));
    dcache = rbh_dcache_new();

    num_groups = backend->fs->super->s_inodes_count
                 / backend->fs->super->s_inodes_per_group;

    ext2fs_init_dblist(backend->fs, &dblist);
    /**
      * necessary mutex lock because ext2fs_open_inode_scan edits then restores
      * the fs struct
      */
    pthread_mutex_lock(&backend->dblist_lock);
    rc = ext2fs_open_inode_scan(backend->fs,
                                backend->fs->inode_blocks_per_group,
                                &iscan);
    pthread_mutex_unlock(&backend->dblist_lock);
    if (rc) {
        *ret = ldiskfs_error("failed to init inode scan: %s",
                             error_message(rc));
        pthread_exit(ret);
    }

    inode_size = EXT2_INODE_SIZE(backend->fs->super);
    inode = malloc(inode_size);
    if (!inode) {
        *ret = false;
        pthread_exit(ret);
    }

    curr_group = thr_id;
    if (curr_group >= num_groups)
        goto free;
    ext2fs_inode_scan_goto_blockgroup(iscan, curr_group);

    rc = ext2fs_get_next_inode_full(iscan, &ino, inode, inode_size);

    while (!rc && (curr_group < num_groups)) {
        if (ino == 0)
            break;

        if (ino < EXT2_GOOD_OLD_FIRST_INO && ino != EXT2_ROOT_INO)
            /* skip reserved inodes except the root */
            goto next;

        if (!ext2fs_fast_test_inode_bitmap2(backend->fs->inode_map, ino))
            /* skip deleted inodes */
            goto next;

        if (LINUX_S_ISDIR(inode->i_mode)) {
            if (!add_dir_blocks(backend, ino, inode, inode_size, dcache, dblist)) {
                ret = false;
                pthread_exit(ret);
            }
        } else {
            dentry = rbh_dcache_find_or_create(dcache, ino);
            dentry->inode = dup_inode(inode, inode_size);
        }

next:
        rc = ext2fs_get_next_inode_full(iscan, &ino, inode, inode_size);
        if (!rc &&
            ino > ((curr_group + 1) * backend->fs->super->s_inodes_per_group)) {
            curr_group += backend->nthreads;
            if (curr_group >= num_groups)
                break;
            ext2fs_inode_scan_goto_blockgroup(iscan, curr_group);
            rc = ext2fs_get_next_inode_full(iscan, &ino, inode, inode_size);
        }
    }

    pthread_mutex_lock(&backend->dcache_lock);
    rbh_dcache_merge(*(void **)dcache, *(void **)(backend->dcache));
    pthread_mutex_unlock(&backend->dcache_lock);

    pthread_mutex_lock(&backend->dblist_lock);
    ext2fs_merge_dblist(dblist, backend->fs->dblist);
    pthread_mutex_unlock(&backend->dblist_lock);

free:
    free(inode);
    rbh_dcache_destroy(dcache);
    ext2fs_close_inode_scan(iscan);
    ext2fs_free_dblist(dblist);

    *ret = true;
    pthread_exit(ret);
}


/* Scan all the inodes from all the groups and fetch inline xattrs (xattrs
 * stored alongside the inode). Inodes containing external xattrs are kept in
 * memory to read them later.
 */
static bool
scan_inodes(struct ldiskfs_backend *backend)
{
    bool ret = true;
    void * thr_ret;
    errcode_t rc;

    rc = ext2fs_read_inode_bitmap(backend->fs);
    if (rc)
        return ldiskfs_error("failed to read inode bitmap: %s",
                             error_message(rc));

    rc = ext2fs_init_dblist(backend->fs, NULL);
    if (rc)
        return ldiskfs_error("failed to init directory block list: %s",
                             error_message(rc));

    pthread_t *threads = malloc(backend->nthreads * sizeof(pthread_t));

    for(int i = 0; i < backend->nthreads; i++)
        pthread_create(&threads[i], NULL, &scan_inodes_thr, backend);

    for(int i = 0; i < backend->nthreads; i++) {
        rc = (pthread_join(threads[i], &thr_ret));
        if (rc)
            return ldiskfs_error("Failed to join thread during inode scan: %s",
                                 error_message(rc));
        ret = ret && *(bool *)thr_ret;

        free(thr_ret);
    }

    free(threads);

    return ret;
}

static void
link_dentry(struct rbh_dentry *parent, struct rbh_dentry *child)
{
    struct rbh_parent_pair *pair = xmalloc(sizeof(pair));


    pair->name = child->name;
    pair->parent = parent;

    child->parent = parent;
    g_queue_push_tail(child->parents, pair);
    parent->children = g_list_prepend(parent->children, child);
}

static int
dblist_iter_cb(ext2_ino_t parent_ino, int entry,
               struct ext2_dir_entry *dentry,
               int offset, int blocksize,
               char *buf, void *private)
{
    struct rbh_dcache *dcache = private;
    struct rbh_dentry *cached_dentry;
    struct rbh_dentry *parent;
    int namelen;

    (void) entry;
    (void) offset;
    (void) blocksize;
    (void) buf;

    if (dentry->inode == 0)
        return 0;

    namelen = ext2fs_dirent_name_len(dentry);
    if ((namelen == 2 && !strcmp(dentry->name, "..")) ||
        (namelen == 1 && !strcmp(dentry->name, ".")))
        return 0;

    parent = rbh_dcache_find(dcache, parent_ino);
    cached_dentry = rbh_dcache_find(dcache, dentry->inode);

    assert(parent && cached_dentry);

    cached_dentry->name = strndup(dentry->name, namelen);
    cached_dentry->namelen = namelen;

    link_dentry(parent, cached_dentry);

    return 0;
}

static bool
scan_dentries(struct ldiskfs_backend *backend)
{
    errcode_t rc;

    rc = ext2fs_dblist_dir_iterate(backend->fs->dblist,
                                   DIRENT_FLAG_INCLUDE_EMPTY,
                                   NULL,
                                   dblist_iter_cb,
                                   backend->dcache);
    if (rc)
        return ldiskfs_error("failed to scan through directory block list: %s",
                             error_message(rc));

    return true;
}

bool
scan_target(struct ldiskfs_backend *backend)
{
    return scan_inodes(backend) &&
        scan_dentries(backend);
}

bool
set_target_type_and_index(ext2_filsys fs, struct ldiskfs_iter *iter)
{
    char volume_name[sizeof(fs->super->s_volume_name) + 1];
    char *target_name;
    long target_index;
    char *index;
    char *dash;
    char *end;
    int rc;

    rc = snprintf(volume_name, sizeof(volume_name), "%.*s",
                  /* expand to length and buf */
                  EXT2_LEN_STR(fs->super->s_volume_name));
    if (rc == -1 || rc > sizeof(volume_name))
        return false;

    dash = strchr(volume_name, '-');
    if (!dash) {
        rbh_backend_error_printf("no '-' found in volume name. Is this a lustre target?");
        return false;
    }

    target_name = dash + 1;
    if (!strncmp(target_name, "MDT", 3)) {
        iter->is_mdt = true;
    } else if (!strncmp(target_name, "OST", 3)) {
        iter->is_mdt = false;
    } else if (!strncmp(target_name, "MGS", 3)) {
        rbh_backend_error_printf("The ldiskfs backend does not support MGT scans.");
        return false;
    } else {
        rbh_backend_error_printf("'%s' is not a lustre target UUID", volume_name);
        return false;
    }

    index = target_name + 3;
    errno = 0;
    target_index = strtol(index, &end, 16);
    if (errno != 0 || *end != '\0') {
        rbh_backend_error_printf("failed to parse lustre target index '%s'", index);
        return false;
    }

    if (target_index > INT_MAX) {
        rbh_backend_error_printf("Lustre target index '%ld' is too big", target_index);
        return false;
    }

    iter->target_index = target_index;

    return true;
}
