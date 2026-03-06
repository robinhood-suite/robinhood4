/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "internals.h"

int
rbh_get_xattrs(char *_name, char *_value,size_t value_len,
               ext2_ino_t inode_num, void *data)
{

    (void) inode_num;

    // FIXME I'm pretty sure this is a memory leak since rbh fsentries are freed using only a call to free()
    struct rbh_value *xattr_value = xmalloc(sizeof(struct rbh_value));
    char *value = xmalloc(value_len * sizeof(char));
    char *name = xmalloc(name_len * sizeof(char));
    size_t name_len = strlen(_name);

    memcpy(value, _value, value_len * sizeof(char));
    memcpy(name, _name, name_len * sizeof(char));

    // Skip the system.data xattr that only contains inline data if it exists
    if (strcmp(name, "system.data") == 0) {
        free(value);
        free(name);
        free(xattr_value);
        return 0;
    }

    struct rbh_value _xattr_value = {
        .type = RBH_VT_STRING,
        .binary = {
            .data = value,
            .size = value_len,
        }
    };

    *xattr_value = _xattr_value;

    struct rbh_value_pair xattr_pair = {
        .key = name,
        .value = xattr_value,
    };

    struct rbh_value_map *xattrs = data;
    struct rbh_value_pair *pairs = (struct rbh_value_pair *)xattrs->pairs;

    pairs[xattrs->count] = xattr_pair;
    xattrs->count++;

    return 0;
}

struct rbh_value_map
get_xattrs_from_inode(ext2_filsys fs, struct ext2_inode_large *inode,
                      ext2_ino_t ino)
{
    struct rbh_value_map default_ret = {0};
    struct ext2_xattr_handle *handle;
    struct rbh_value_pair *pairs;
    struct rbh_value_map ret;
    errcode_t err;
    size_t count;

    err = ext2fs_xattrs_open(fs, ino, &handle);
    if (err)
        goto err;

    err = ext2fs_xattrs_read(handle);
    if (err)
        goto err;

    err = ext2fs_xattrs_count(handle, &count);
    if (err)
        goto err;

    pairs = xmalloc(count * sizeof(struct rbh_value_pair));

    ret.count = 0;
    ret.pairs = pairs;

    err = ext2fs_xattrs_iterate(handle, &rbh_get_xattrs, &ret);
    if (err)
        goto err;

    err = ext2fs_xattrs_close(&handle);
    if (err)
        goto err;

    return ret;

err:
    return default_ret;
}
