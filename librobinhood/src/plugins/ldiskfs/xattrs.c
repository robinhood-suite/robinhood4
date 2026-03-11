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
    name[strlen(name)] = '\0';

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
