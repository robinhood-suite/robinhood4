/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "internals.h"
#include <endian.h>

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
        __le32 leh_magic;
        __le32 leh_reccount;
        __le64 leh_len;
        __le32 leh_overflow_time;
        __le32 leh_padding;
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

    // Skip the system.data xattr that only contains inline data if it exists
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
        goto err;

    err = ext2fs_xattrs_count(handle, &count);
    if (err)
        goto err;

    pairs = rbh_sstack_alloc(sstack, NULL, count * sizeof(struct rbh_value_pair));

    ret.count = 0;
    ret.pairs = pairs;

    err = ext2fs_xattrs_iterate(handle, rbh_get_xattrs, &data);
    if (err)
        goto err;

    err = ext2fs_xattrs_close(&handle);
    if (err)
        goto err;

    return ret;

err:
    return default_ret;
}

struct lu_fid
lu_fid_from_lma(void *lma)
{

    struct lustre_mdt_attrs *attrs;
    struct lu_fid res;

    attrs = (void *)lma;
    res = attrs->lma_self_fid;

    return res;

}

struct lu_fid
lu_fid_from_filter_fid(void *fid)
{

    struct filter_fid *fid_s;
    struct lu_fid res;

    fid_s = (void *)fid;
    res = fid_s->ff_parent;

    return res;

}


 /*
 * The trusted.link extended attribute describes the hard links to a file.
 * It has the following structure:
 *
 * ------------------------------------------------------------
 * |     header     |   entry    |   entry    |   entry    |
 * |-----------------------------------------------------------
 * | magic          | reclen     | reclen     | reclen     |
 * | reccount       | parent_fid | parent_fid | parent_fid |
 * | len            | name       | name       | name       |
 * | overflow_time  |            |            |            |
 * | padding        |            |            |            | ...
 * ------------------------------------------------------------
 *
 * An example of a header and the following entries would be:
 *
 * for `file0` and `file1`, both located in `/`:
 *
 * dff1ea110200000046000000000000000000000000000000 //header
 * 00170000000200000007000000010000000066696c6530   //first entry
 * 00170000000200000007000000010000000066696c6531   //second entry
 *
 * --------------------------------------
 * |               header               |
 * |------------------------------------|
 * | magic         | 0x11eaf1df         | -> magic number, always the same
 * | reccount      | 0x00000002         | -> number of entries following the header
 * | len           | 0x0000000000000046 | -> length of the whole extended attribute
 * | overflow_time | 0x00000000         |
 * | padding       | 0x00000000         |
 * --------------------------------------
 *
 *  Note that each attribute is stored in little endian notation in the header,
 *  however in the entries, big endian notation is used.
 *
 * ---------------------------------------
 * |             first entry             |
 * |-------------------------------------|
 * | reclen         | 0x0017             | -> length of the entry, big endian notation
 * | parent_fid     | 0x0000000200000007 | -> Lustre FID of the parent directory
 * |                | 0x00000001         |    here [0x200000007:0x1:0x0]
 * |                | 0x00000000         |
 * | name           | 0x66696c6530       | -> name of the file ("file0")
 * ---------------------------------------
 *
 * ---------------------------------------
 * |            second entry             |
 * |-------------------------------------|
 * | reclen         | 0x0017             | -> length of the entry, big endian notation
 * | parent_fid     | 0x0000000200000007 | -> Lustre FID of the parent directory
 * |                | 0x00000001         |    here [0x200000007:0x1:0x0]
 * |                | 0x00000000         |
 * | name           | 0x66696c6531       | -> name of the file ("file1")
 * ---------------------------------------
 *
 */


struct rbh_value_map
parents_lu_fid_from_link(void *link, struct rbh_sstack *sstack)
{
    struct rbh_value_map default_ret = {0};
    struct link_ea_header *link_header;
    struct link_ea_entry *link_entry;
    struct rbh_value_pair *pairs;
    struct rbh_value_pair *pair;
    struct lu_fid *parent_fid;
    struct rbh_value_map ret;
    size_t name_len;
    __u16 curr_len;
    char *name;
    int rc;

    link_header = (void *)link;

    if(link_header->leh_magic != LINK_EA_MAGIC) {
        rbh_backend_error_printf("Extended attribute trusted.links has invalid magic number");
        goto err;
    }

    link_entry = (void *)link + sizeof(struct link_ea_header);

    pairs = rbh_sstack_alloc(sstack, NULL, link_header->leh_reccount *
                             sizeof(struct rbh_value_pair));

    ret.count = 0;
    ret.pairs = pairs;

    while ((size_t)link_entry < (size_t)link_header + link_header->leh_len) {
        pair = &pairs[ret.count];

        // length of the current record
        curr_len = (link_entry->lee_reclen[0] << 8) + link_entry->lee_reclen[1];
        // length of the record's name
        name_len = curr_len - (sizeof(link_entry->lee_reclen) +
                                   sizeof(struct lu_fid));

        name = rbh_sstack_push(sstack, NULL, name_len + 1);
        memcpy(name, link_entry->lee_name, name_len);
        name[name_len] = '\0';

        rc = fill_binary_pair(name, &(link_entry->lee_parent_fid),
                              sizeof(struct lu_fid), pair, sstack);
        if(rc)
            goto err;

        parent_fid =(struct lu_fid *)pair->value->binary.data;
        // data is big endian in the xattr
        // so we change the value to the right endianness
        parent_fid->f_seq = be64toh(parent_fid->f_seq);
        parent_fid->f_oid = be32toh(parent_fid->f_oid);
        parent_fid->f_ver = be32toh(parent_fid->f_ver);

        ret.count++;
        link_entry = (void *)((size_t)link_entry + curr_len);
    }
    return ret;

err:
    return default_ret;
}

struct lu_fid
get_fid_from_xattrs(struct rbh_value_map *xattrs)
{
    struct lu_fid ret = {0};
    int i;

    for(i = 0; i < xattrs->count; i++) {
        if (!strcmp(xattrs->pairs[i].key, "trusted.lma")) {
            ret = lu_fid_from_lma((char *)xattrs->pairs[i].value->string);
            break;
        }
    }

    return ret;
}
