/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "internals.h"
#include <endian.h>
#include <linux/swab.h>

#define LINK_EA_MAGIC 0x11EAF1DFUL

struct xattr_iter_data {
    struct rbh_value_map *values;
    struct rbh_sstack *sstack;
};

/*
 * Internal Lustre structures needed to use the trusted.link xattr
 * Copied from Lustre source code and modified for our use
 * Original file is lustre/include/uapi/linux/lustre/lustre_idl.h
 */
struct link_ea_entry {
        __u8      lee_reclen[2]; /* __u16 big-endian, unaligned */
        struct lu_fid      lee_parent_fid;
        char               lee_name[];
} __attribute__((packed));

struct link_ea_header {
        __u32 leh_magic;
        __u32 leh_reccount;
        __u64 leh_len;
        __u32 leh_overflow_time;
        __u32 leh_padding;
};

int
rbh_get_xattrs(char *_name, char *value,size_t value_len,
               ext2_ino_t inode_num, void *_data)
{
    struct xattr_iter_data *data = _data;
    struct rbh_value_pair *xattr_pair;
    struct rbh_value_map *xattrs;
    struct rbh_value_pair *pairs;
    char *name;
    int rc;

    (void) inode_num;

    xattrs = data->values;
    pairs = (struct rbh_value_pair *)xattrs->pairs;
    xattr_pair = &pairs[xattrs->count];

    // The system.data extended attribute stores inline data if it does not fit
    // in the ext4 inode
    // Lustre does not store metadata in it so we do not need it
    // and can skip it safely
    if (!strcmp(_name, "system.data"))
        return 0;

    // fill_binary_pair does not push the name on the sstack so we do it manually.
    name = rbh_sstack_push(data->sstack, _name, strlen(_name) + 1);

    rc = fill_binary_pair(name, value, value_len, xattr_pair, data->sstack);
    if (rc)
        return -1;

    xattrs->count++;

    return 0;
}

struct rbh_value_map
get_xattrs_from_inode(ext2_filsys fs, struct ext2_inode_large *inode,
                      ext2_ino_t ino, struct rbh_sstack *sstack)
{
    struct rbh_value_map default_ret = {0};
    struct ext2_xattr_handle *handle;
    struct rbh_value_pair *pairs;
    struct xattr_iter_data data;
    struct rbh_value_map ret;
    errcode_t err;
    size_t count;

    data.values = &ret;
    data.sstack = sstack;

    err = ext2fs_xattrs_open(fs, ino, &handle);
    if (err)
        goto err;

    err = ext2fs_xattrs_read(handle);
    if (err)
        goto err2;

    err = ext2fs_xattrs_count(handle, &count);
    if (err)
        goto err2;

    pairs = rbh_sstack_alloc(sstack, NULL, count * sizeof(struct rbh_value_pair));

    ret.count = 0;
    ret.pairs = pairs;

    // we don't change any xattr value so the return value will always be 0
    ext2fs_xattrs_iterate(handle, &rbh_get_xattrs, &data);

    err = ext2fs_xattrs_close(&handle);
    if (err)
        goto err;

    return ret;

err2:
    ext2fs_xattrs_close(&handle);

err:
    rbh_backend_error_printf("Failed to read extended attributes of inode %d",
                            ino);
    return default_ret;
}

// use right endianness for attributes of struct lu_fid
void set_lu_fid_with_right_endianness(struct lu_fid *fid)
{
    fid->f_seq = le64toh(fid->f_seq);
    fid->f_oid = le32toh(fid->f_oid);
    fid->f_ver = le32toh(fid->f_ver);
}

struct lu_fid
lu_fid_from_lma(void *lma)
{

    struct lustre_mdt_attrs *attrs;
    struct lu_fid res;

    attrs = (void *)lma;
    res = attrs->lma_self_fid;

    // FIDs are in little endian notation on disk so we use the right endianness
    set_lu_fid_with_right_endianness(&res);

    return res;
}

bool
get_fid_from_xattrs(struct rbh_value_map *xattrs, struct lu_fid *fid)
{
    for(int i = 0; i < xattrs->count; i++) {
        if (!strcmp(xattrs->pairs[i].key, "trusted.lma")) {
            *fid = lu_fid_from_lma((char *)xattrs->pairs[i].value->string);
            return true;
        }
    }
    return false;
}
