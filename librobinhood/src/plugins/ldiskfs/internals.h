/* This file is part of RobinHood
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_LDISKFS_INTERNALS_H
#define ROBINHOOD_LDISKFS_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/backends/ldiskfs.h>
#include <robinhood/utils.h>
#include <robinhood/sstack.h>
#include <value.h>
#include <ext2fs/ext2fs.h>
#include <lustre/lustre_user.h>

#include "dcache.h"

#define ldiskfs_error(fmt, ...) \
    ({                                                         \
        rbh_backend_error_printf("ldiskfs: " fmt __VA_OPT__(,) \
                                 __VA_ARGS__);                 \
        false;                                                 \
    })

struct ldiskfs_backend {
    struct rbh_backend backend;
    ext2_filsys fs;
    ext2_inode_scan iscan;
    struct rbh_dcache *dcache;
};

struct ldiskfs_iter {
    struct rbh_mut_iterator iter;
    struct rbh_sstack *sstack;
    bool is_mdt;
    uint32_t target_index;
    struct rbh_dentry *root;
    struct rbh_dentry *remote_parent_dir;
    struct rbh_dentry *current;
    ext2_filsys fs;
    GQueue *tasks;
    GList *current_dir;
};

// Internal Lustre structures needed to use the trusted.link xattr
// Copied from Lustre source code and modified for our use
struct link_ea_entry {
        __u8      lee_reclen[2]; /* __u16 big-endian, unaligned */
        struct lu_fid      lee_parent_fid;
        char               lee_name[];
} __attribute__((packed));

struct link_ea_header {
        __le32 leh_magic;
        __le32 leh_reccount;
        __le64 leh_len;
        __le32 leh_overflow_time;
        __le32 leh_padding;
};

struct rbh_mut_iterator *
ldiskfs_backend_filter(void *backend, const struct rbh_filter *filter,
                       const struct rbh_filter_options *options,
                       const struct rbh_filter_output *output,
                       struct rbh_metadata *metadata);

void
ldiskfs_backend_destroy(void *backend);

struct rbh_value_map *
ldiskfs_backend_get_info(void *backend, int info_flags);

bool
scan_target(struct ldiskfs_backend *backend);

bool
set_target_type_and_index(ext2_filsys fs, struct ldiskfs_iter *iter);

struct rbh_value_map
get_xattrs_from_inode(ext2_filsys fs, struct ext2_inode_large *inode,
                      ext2_ino_t ino, struct rbh_sstack *sstack);

const struct lu_fid
lu_fid_from_lma(char *lma);

const struct lu_fid
lu_fid_from_fid(char *fid);

const struct rbh_value_map
parents_lu_fid_from_link(char *link, struct rbh_sstack *sstack);

struct lu_fid
get_fid_from_xattrs(struct rbh_value_map xattrs);

#endif
