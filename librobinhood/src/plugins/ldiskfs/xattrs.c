/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "internals.h"

struct xattr_iter_data {
    struct rbh_value_map *values;
    struct rbh_sstack *sstack;
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

    if (!strcmp(_name, "system.data"))
        return 0;

    // fill_binary_pair does not push the name on the sstack so we do it manually.
    name = rbh_sstack_push(data->sstack, _name, strlen(_name) + 1);

    rc = fill_binary_pair(name, value, value_len, xattr_pair, data->sstack);
    if(rc)
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
lu_fid_from_lma(char *lma)
{

    struct lustre_mdt_attrs *attrs;
    struct lu_fid res;

    attrs = (void *)lma;
    res = attrs->lma_self_fid;

    return res;

}

struct lu_fid
lu_fid_from_fid(char *fid)
{

    struct filter_fid *fid_s;
    struct lu_fid res;

    fid_s = (void *)fid;
    res = fid_s->ff_parent;

    return res;

}

struct rbh_value_map
parents_lu_fid_from_link(char *link, struct rbh_sstack *sstack)
{
    struct rbh_value_map default_ret = {0};
    struct rbh_value_pair *pairs;
    struct rbh_value_pair *pair;
    struct link_ea_header *leh;
    struct link_ea_entry *lee;
    struct lu_fid *parent_fid;
    struct rbh_value_map ret;
    size_t name_len;
    __u16 curr_len;
    char *name;
    int rc;

    leh = (void *)link;

    if(leh->leh_magic != 0x11eaf1df) {
        rbh_backend_error_printf("Extended attribute trusted.links has invalid magic number");
        goto err;
    }

    lee = (void *)link + sizeof(struct link_ea_header);

    pairs = rbh_sstack_alloc(sstack, NULL, leh->leh_reccount *
                             sizeof(struct rbh_value_pair));

    ret.count = 0;
    ret.pairs = pairs;

    while((size_t)lee < (size_t)leh + leh->leh_len) {
        pair = &pairs[ret.count];

        // length of the current record
        curr_len = (lee->lee_reclen[0] << 8) + lee->lee_reclen[1];
        // length of the record's name
        name_len = curr_len - (sizeof(lee->lee_reclen) +
                                   sizeof(struct lu_fid));

        name = rbh_sstack_push(sstack, lee->lee_name, name_len + 1);
        name[name_len] = '\0';

        rc = fill_binary_pair(name, &(lee->lee_parent_fid),
                              sizeof(struct lu_fid), pair, sstack);
        if(rc)
            goto err;

        parent_fid =(struct lu_fid *)pair->value->binary.data;
        // data is big endian in the xattr but lu_fid uses little endian notation
        // so we swap the bytes order of each fid field
        parent_fid->f_seq = __bswap_64(parent_fid->f_seq);
        parent_fid->f_oid = __bswap_32(parent_fid->f_oid);
        parent_fid->f_ver = __bswap_32(parent_fid->f_ver);

        ret.count++;
        lee = (void *)((size_t)lee + curr_len);
    }
    return ret;

err:
    return default_ret;
}

struct lu_fid
get_fid_from_xattrs(struct rbh_value_map xattrs)
{
    struct lu_fid ret = {0};
    int i;

    for(i = 0; i < xattrs.count; i++) {
        if (!strcmp(xattrs.pairs[i].key, "trusted.lma")) {
            ret = lu_fid_from_lma((char *)xattrs.pairs[i].value->string);
            break;
        }
    }

    return ret;
}
