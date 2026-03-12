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

/*
 * Gets the lustre fid of a file from the value of the trusted.lma
 * extended attribute.
 *
 * The extended attribute is passed as a pointer to its binary data.
 */
const struct lu_fid
lu_fid_from_lma(void *lma);

/*
 * Gets the fid of the parent of a file from the value of the trusted.fid
 * extended attribute.
 *
 * The extended attribute is passed as a pointer to its binary data.
 *
 * This is useful for ost scanning as `trusted.fid` holds the fid of the parent
 * file found on mdt.
 */
const struct lu_fid
lu_fid_from_filter_fid(void *fid);

/*
 * Obtain the fid of the parents of a file from the value of the trusted.link
 * extended attribute.
 *
 * The value is returned as a rbh_value_map that contains the names of all links
 * refering to a file and the fid of the parents of each of these links.
 *
 * The attribute is passed as a pointer to its binary data.
 */
const struct rbh_value_map
parents_lu_fid_from_link(void *link, struct rbh_sstack *sstack);

/*
 * Obtain the fid of a file from the extended attributes.
 *
 * Extended attributes are passed as a rbh_value_map as returned by
 * get_xattrs_from_inode()
 *
 * Returns true and sets the 'fid' variable if a a Lustre fid was found, else
 * returns false.
 */
bool
get_fid_from_xattrs(struct rbh_value_map *xattrs, struct lu_fid *fid);

#endif
