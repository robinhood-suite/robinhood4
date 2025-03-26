/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

#include <robinhood/statx.h>

const struct rbh_id ROOT_ID = {
    .data = NULL,
    .size = 0,
};

static void *
sqlite_iter_next(void *iterator)
{
    struct sqlite_iterator *iter = iterator;
    struct rbh_value_map inode_xattrs = {0};
    struct rbh_value_map ns_xattrs = {0};
    const char *inode_xattrs_json;
    struct sqlite_cursor *cursor;
    struct rbh_fsentry tmp = {0};
    struct rbh_fsentry *fsentry;
    const char *ns_xattrs_json;
    const char *symlink = NULL;
    struct rbh_statx statx;
    int save_errno;

    cursor = &iter->cursor;

    if (iter->done) {
        errno = ENODATA;
        return NULL;
    }

    if (!sqlite_cursor_step(cursor))
        return NULL;

    if (errno == 0) {
        /* sqlite_cursor_step returns true with errno = EAGAIN if more rows
         * are availble. errno = 0 means that their is no more data to read.
         */
        errno = ENODATA;
        /* sqlite3_step seems to go back to the beginning if called again after
         * having stepped through all the rows. Set done to true to remember the
         * first time we've reached the end.
         */
        iter->done = true;
        return NULL;
    }

    if (!(sqlite_cursor_get_id(cursor, &tmp.id) &&
          sqlite_cursor_get_id(cursor, &tmp.parent_id)))
        return NULL;

    tmp.name = sqlite_cursor_get_string(cursor);

    statx.stx_mask = sqlite_cursor_get_uint32(cursor);
    statx.stx_blksize = sqlite_cursor_get_uint32(cursor);

    statx.stx_nlink = sqlite_cursor_get_uint32(cursor);
    statx.stx_uid = sqlite_cursor_get_uint32(cursor);
    statx.stx_gid = sqlite_cursor_get_uint32(cursor);
    statx.stx_mode =
        sqlite_cursor_get_uint16(cursor) |
        sqlite_cursor_get_uint16(cursor);

    statx.stx_ino = sqlite_cursor_get_uint64(cursor);
    statx.stx_size = sqlite_cursor_get_uint64(cursor);
    statx.stx_blocks = sqlite_cursor_get_uint64(cursor);
    statx.stx_attributes = sqlite_cursor_get_uint64(cursor);
    statx.stx_atime.tv_sec = sqlite_cursor_get_int64(cursor);

    statx.stx_atime.tv_nsec = sqlite_cursor_get_uint32(cursor);
    statx.stx_btime.tv_sec = sqlite_cursor_get_int64(cursor);
    statx.stx_btime.tv_nsec = sqlite_cursor_get_uint32(cursor);
    statx.stx_ctime.tv_sec = sqlite_cursor_get_int64(cursor);
    statx.stx_ctime.tv_nsec = sqlite_cursor_get_uint32(cursor);

    statx.stx_mtime.tv_sec = sqlite_cursor_get_int64(cursor);
    statx.stx_mtime.tv_nsec = sqlite_cursor_get_uint32(cursor);
    statx.stx_rdev_major = sqlite_cursor_get_uint32(cursor);
    statx.stx_rdev_minor = sqlite_cursor_get_uint32(cursor);
    statx.stx_dev_major = sqlite_cursor_get_uint32(cursor);

    statx.stx_dev_minor = sqlite_cursor_get_uint32(cursor);
    statx.stx_mnt_id = sqlite_cursor_get_uint64(cursor);

    inode_xattrs_json = sqlite_cursor_get_string(cursor);
    ns_xattrs_json = sqlite_cursor_get_string(cursor);
    if (!(sqlite_json2xattrs(inode_xattrs_json, &inode_xattrs) &&
          sqlite_json2xattrs(ns_xattrs_json, &ns_xattrs))) {
        fsentry = NULL;
        goto out_free;
    }
    symlink = sqlite_cursor_get_string(cursor);

    errno = ENODATA;
    fsentry = rbh_fsentry_new(&tmp.id,
                              tmp.parent_id.size > 0 ?
                                &tmp.parent_id :
                                &ROOT_ID,
                              tmp.name,
                              &statx,
                              &ns_xattrs,
                              &inode_xattrs,
                              symlink);

out_free:
    save_errno = errno;
    free((void *)tmp.name);
    free((void *)tmp.id.data);
    free((void *)tmp.parent_id.data);
    free((void *)symlink);
    errno = save_errno;

    return fsentry;
}

static void
sqlite_iter_destroy(void *iterator)
{
    struct sqlite_iterator *iter = iterator;

    if (iter->cursor.stmt)
        sqlite_cursor_fini(&iter->cursor);
    free(iter);
}

static const struct rbh_mut_iterator_operations SQLITE_ITER_OPS = {
    .next    = sqlite_iter_next,
    .destroy = sqlite_iter_destroy,
};

static const struct rbh_mut_iterator SQLITE_ITER = {
    .ops = &SQLITE_ITER_OPS,
};

static struct sqlite_iterator *
sqlite_iterator_new(void)
{
    struct sqlite_iterator *iter;

    iter = malloc(sizeof(*iter));
    if (!iter)
        return NULL;

    iter->iter = SQLITE_ITER;
    iter->cursor.stmt = NULL;

    return iter;
}

struct rbh_mut_iterator *
sqlite_backend_filter(void *backend, const struct rbh_filter *filter,
                      const struct rbh_filter_options *options,
                      const struct rbh_filter_output *output)
{
    struct sqlite_iterator *iter = sqlite_iterator_new();

    if (!iter)
        return NULL;

    return &iter->iter;
}
