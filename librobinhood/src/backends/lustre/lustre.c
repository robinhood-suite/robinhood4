/**
 * This file is part of RobinHood 4
 *
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 * alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#include "plugins/backend.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <lustre/lustreapi.h>
#include <lustre/lustre_user.h>
#include <linux/lustre/lustre_idl.h>

#include "robinhood/backends/posix.h"
#include "robinhood/utils.h"
#include "robinhood/backends/posix_internal.h"
#include "robinhood/backends/posix_extension.h"
#include "robinhood/backends/lustre_internal.h"
#include "robinhood/backends/lustre.h"
#include "robinhood/statx.h"
#include "value.h"

#ifndef HAVE_LUSTRE_FILE_HANDLE
/* This structure is not defined before 2.15, so we define it to retrieve the
 * fid of a file
 */
struct lustre_file_handle {
    struct lu_fid lfh_child;
    struct lu_fid lfh_parent;
};
#endif

struct iterator_data {
    struct rbh_value *stripe_count;
    struct rbh_value *stripe_size;
    struct rbh_value *mirror_id;
    struct rbh_value *pattern;
    struct rbh_value *begin;
    struct rbh_value *flags;
    struct rbh_value *pool;
    struct rbh_value *end;
    struct rbh_value *ost;
    int comp_index;
    int ost_size;
    int ost_idx;
};

static __thread struct rbh_value_pair *_inode_xattrs;
static __thread ssize_t *_inode_xattrs_count;
static __thread struct rbh_sstack *_values;
static __thread uint16_t mode;

static enricher_t retention_enricher;

/**
 * Record a file's fid in \p pairs
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
static int
xattrs_get_fid(int fd, struct rbh_value_pair *pairs, int available_pairs)
{
    size_t handle_size = sizeof(struct lustre_file_handle);
    static struct file_handle *handle;
    int mount_id;
    int rc;

    if (available_pairs < 1)
        return -1;

    if (handle == NULL) {
        /* Per-thread initialization of `handle' */
        handle = malloc(sizeof(*handle) + handle_size);
        if (handle == NULL)
            return -1;
        handle->handle_bytes = handle_size;
    }

retry:
    handle->handle_bytes = handle_size;
    if (name_to_handle_at(fd, "", handle, &mount_id, AT_EMPTY_PATH)) {
        struct file_handle *tmp;

        if (errno != EOVERFLOW || handle->handle_bytes <= handle_size)
            return -1;

        tmp = malloc(sizeof(*tmp) + handle->handle_bytes);
        if (tmp == NULL)
            return -1;

        handle_size = handle->handle_bytes;
        free(handle);
        handle = tmp;
        goto retry;
    }

    rc = fill_binary_pair(
        "fid", &((struct lustre_file_handle *)handle->f_handle)->lfh_child,
        sizeof(struct lu_fid), pairs, _values
    );

    return rc ? : 1;
}

/**
 * Record a file's hsm attributes (state and archive_id) in \p pairs
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
static int
xattrs_get_hsm(int fd, struct rbh_value_pair *pairs, int available_pairs)
{
    struct hsm_user_state hus;
    int subcount = 0;
    int rc;

    if (!S_ISREG(mode))
        /* Only regular files can be archived */
        return 0;

    if (available_pairs < 2)
        return -1;

    rc = llapi_hsm_state_get_fd(fd, &hus);
    if (rc && rc != -ENODATA) {
        errno = -rc;
        return -1;
    }

    if (rc == -ENODATA || (hus.hus_archive_id == 0 && hus.hus_states == 0))
        return 0;

    rc = fill_uint32_pair("hsm_state", hus.hus_states, &pairs[subcount++],
                          _values);
    if (rc)
        return -1;

    rc = fill_uint32_pair("hsm_archive_id", hus.hus_archive_id,
                          &pairs[subcount++], _values);
    if (rc)
        return -1;

    return subcount;
}

static inline struct rbh_value
create_uint64_value(uint64_t integer)
{
    struct rbh_value val = {
        .type = RBH_VT_UINT64,
        .uint64 = integer,
    };

    return val;
}

static inline struct rbh_value
create_uint32_value(uint32_t integer)
{
    struct rbh_value val = {
        .type = RBH_VT_UINT32,
        .uint32 = integer,
    };

    return val;
}

static inline struct rbh_value
create_string_value(char *str, size_t len)
{
    struct rbh_value val = {
        .type = RBH_VT_STRING,
        .string = RBH_SSTACK_PUSH(_values, str, len),
    };

    return val;
}

/**
 * Resize the OST array in \p data if the list of the current component's OSTs
 * cannot fit
 *
 * @param data      iterator_data structure with OST array to resize
 * @param index     index corresponding to the position to fill in the \p data
 *                  arrays
 *
 * @return          -1 if an error occured (and errno is set), 0 otherwise
 */
static int
iter_data_ost_try_resize(struct iterator_data *data, int ost_len)
{
    void *tmp;

    if (data->ost_idx + ost_len > data->ost_size) {
        tmp = realloc(data->ost,
                      (data->ost_size + ost_len) * sizeof(*data->ost));
        if (tmp == NULL) {
            errno = -ENOMEM;
            return -1;
        }

        data->ost = tmp;
        data->ost_size += ost_len;
    }

    return 0;
}

/**
 * Fill \p data using \p layout attributes
 *
 * @param layout    layout to retrieve the attributes from
 * @param data      iterator_data structure to fill
 * @param index     index corresponding to the position to fill in the \p data
 *                  arrays
 *
 * @return          -1 if an error occured (and errno is set), 0 otherwise
 */
static int
fill_iterator_data(struct llapi_layout *layout,
                   struct iterator_data *data, const int index)
{
    char pool_tmp[LOV_MAXPOOLNAME + 1];
    uint64_t stripe_count = 0;
    bool is_init_or_not_comp;
    uint32_t flags = 0;
    uint64_t tmp = 0;
    int ost_len;
    int rc;

    rc = llapi_layout_stripe_count_get(layout, &stripe_count);
    if (rc)
        return -1;

    data->stripe_count[index] = create_uint64_value(stripe_count);

    rc = llapi_layout_stripe_size_get(layout, &tmp);
    if (rc)
        return -1;

    data->stripe_size[index] = create_uint64_value(tmp);

    rc = llapi_layout_pattern_get(layout, &tmp);
    if (rc)
        return -1;

    data->pattern[index] = create_uint64_value(tmp);

    rc = llapi_layout_comp_flags_get(layout, &flags);
    if (rc)
        return -1;

    data->flags[index] = create_uint32_value(flags);

    rc = llapi_layout_pool_name_get(layout, pool_tmp, sizeof(pool_tmp));
    if (rc)
        return -1;

    data->pool[index] = create_string_value(pool_tmp, strlen(pool_tmp) + 1);

    if (S_ISDIR(mode))
        /* XXX we do not yet fetch the OST indexes of directories */
        return 0;

    is_init_or_not_comp = (flags == LCME_FL_INIT ||
                           !llapi_layout_is_composite(layout));
    ost_len = (is_init_or_not_comp ? stripe_count : 1);

    rc = iter_data_ost_try_resize(data, ost_len);
    if (rc)
        return rc;

    if (is_init_or_not_comp) {
        uint64_t i;

        for (i = 0; i < stripe_count; ++i) {
            rc = llapi_layout_ost_index_get(layout, i, &tmp);
            if (rc == -1 && errno == EINVAL)
                break;
            else if (rc)
                return rc;

            data->ost[data->ost_idx++] = create_uint64_value(tmp);
        }
    } else {
        data->ost[data->ost_idx++] = create_uint64_value(-1);
    }

    return 0;
}

/**
 * Callback function called by Lustre when iterating over a layout's components
 *
 * @param layout    layout of the file, with the current component set by Lustre
 * @param cbdata    void pointer pointing to callback parameters
 *
 * @return          -1 on error, 0 otherwise
 */
static int
xattrs_layout_iterator(struct llapi_layout *layout, void *cbdata)
{
    struct iterator_data *data = (struct iterator_data*) cbdata;
    uint32_t mirror_id;
    uint64_t begin;
    uint64_t end;
    int rc;

    rc = fill_iterator_data(layout, data, data->comp_index);
    if (rc)
        return -1;

    rc = llapi_layout_comp_extent_get(layout, &begin, &end);
    if (rc)
        return -1;

    data->begin[data->comp_index] = create_uint64_value(begin);
    data->end[data->comp_index] = create_uint64_value(end);

    rc = llapi_layout_mirror_id_get(layout, &mirror_id);
    if (rc)
        return -1;

    data->mirror_id[data->comp_index] = create_uint32_value(mirror_id);

    data->comp_index += 1;

    return 0;
}

static int
layout_get_nb_comp(struct llapi_layout *layout, void *cbdata)
{
    uint32_t *nb_comp = (uint32_t *) cbdata;

    (*nb_comp)++;

    return 0;
}

static int
init_iterator_data(struct iterator_data *data, const uint32_t length,
                   const int nb_xattrs)
{
    /**
     * We want to fetch 8 attributes: mirror_id, stripe_count, stripe_size,
     * pattern, begin, end, flags, pool.
     *
     * Additionnaly, we will keep the OSTs of each component in a separate list
     * because its length isn't fixed.
     */
    struct rbh_value *arr = calloc(nb_xattrs * length, sizeof(*arr));
    if (arr == NULL)
        return -1;

    data->stripe_count = arr;
    data->stripe_size = &arr[1 * length];
    data->pattern = &arr[2 * length];
    data->flags = &arr[3 * length];
    data->pool = &arr[4 * length];

    if (nb_xattrs >= 6) {
        data->mirror_id = &arr[5 * length];
        data->begin = &arr[6 * length];
        data->end = &arr[7 * length];
    }

    if (S_ISDIR(mode)) {
        /* XXX we do not yet fetch the OST indexes of directories */
        data->ost = NULL;
    } else {
        data->ost = malloc(length * sizeof(*data->ost));
        if (data->ost == NULL) {
            free(arr);
            errno = -ENOMEM;
            return -1;
        }
    }

    data->ost_size = length;
    data->ost_idx = 0;
    data->comp_index = 0;

    return 0;
}

static int
xattrs_fill_layout(struct iterator_data *data, int nb_xattrs,
                   struct rbh_value_pair *pairs)
{
    struct rbh_value *values[] = {data->stripe_count, data->stripe_size,
                                  data->pattern, data->flags, data->pool,
                                  data->mirror_id, data->begin, data->end};
    char *keys[] = {"stripe_count", "stripe_size", "pattern", "comp_flags",
                    "pool", "mirror_id", "begin", "end"};
    int subcount = 0;
    int rc;

    for (int i = 0; i < nb_xattrs; ++i) {
        rc = fill_sequence_pair(keys[i], values[i], data->comp_index,
                                &pairs[subcount++], _values);
        if (rc)
            return -1;
    }

    if (S_ISDIR(mode))
        /* XXX we do not yet fetch the OST indexes of directories */
        return subcount;

    rc = fill_sequence_pair("ost", data->ost, data->ost_idx,
                            &pairs[subcount++], _values);
    if (rc)
        return -1;

    return subcount;
}

static int
_xattrs_get_magic_and_gen(int fd, const char *lov_buf,
                          struct rbh_value_pair *pairs)
{
    uint32_t magic = 0;
    int subcount = 0;
    uint32_t gen = 0;
    char *magic_str;
    int rc;

    (void) fd;

    magic = ((struct lov_user_md *) lov_buf)->lmm_magic;

    switch (magic) {
    case LOV_USER_MAGIC_V1:
        magic_str = "LOV_USER_MAGIC_V1";
        gen = ((struct lov_user_md_v1 *) lov_buf)->lmm_layout_gen;
        break;
    case LOV_USER_MAGIC_COMP_V1:
        magic_str = "LOV_USER_MAGIC_COMP_V1";
        gen = ((struct lov_comp_md_v1 *) lov_buf)->lcm_layout_gen;
        break;
#ifdef HAVE_LOV_USER_MAGIC_SEL
    case LOV_USER_MAGIC_SEL:
        magic_str = "LOV_USER_MAGIC_SEL";
        gen = ((struct lov_comp_md_v1 *) lov_buf)->lcm_layout_gen;
        break;
#endif
    case LOV_USER_MAGIC_V3:
        magic_str = "LOV_USER_MAGIC_V3";
        gen = ((struct lov_user_md_v3 *) lov_buf)->lmm_layout_gen;
        break;
    case LOV_USER_MAGIC_SPECIFIC:
        magic_str = "LOV_USER_MAGIC_SPECIFIC";
        gen = ((struct lov_user_md_v3 *) lov_buf)->lmm_layout_gen;
        break;
#ifdef HAVE_LOV_USER_MAGIC_FOREIGN
    case LOV_USER_MAGIC_FOREIGN:
        magic_str = "LOV_USER_MAGIC_FOREIGN";
        gen = -1;
        break;
#endif
    default:
        errno = EINVAL;
        return -1;
    }

    rc = fill_string_pair("magic", magic_str, &pairs[subcount++], _values);
    if (rc)
        return -1;

    rc = fill_uint32_pair("gen", gen, &pairs[subcount++], _values);
    if (rc)
        return -1;

    return subcount;
}

/* The Linux VFS does not allow values of more than 64KiB */
static const size_t XATTR_VALUE_MAX_VFS_SIZE = 1 << 16;

/**
 * Record a file's magic number and layout generation in \p pairs
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
static int
xattrs_get_magic_and_gen(int fd, struct rbh_value_pair *pairs)
{
    char buffer[XATTR_VALUE_MAX_VFS_SIZE];
    const char *lov_buf = NULL;

    if (_inode_xattrs != NULL) {
        for (int i = 0; i < *_inode_xattrs_count; ++i) {
            if (!strcmp(_inode_xattrs[i].key, XATTR_LUSTRE_LOV)) {
                lov_buf = _inode_xattrs[i].value->binary.data;
                break;
            }
        }
    } else {
    /* TODO: when the attribute will be retrieved using the changelog,
     * change the xattr retrieval by seeking the one already retrieved.
     */
        ssize_t length = XATTR_VALUE_MAX_VFS_SIZE;

        length = fgetxattr(fd, XATTR_LUSTRE_LOV, buffer, length);
        if (length == -1)
            return -1;

        lov_buf = buffer;
    }

    if (!lov_buf)
        return 0;

    return _xattrs_get_magic_and_gen(fd, lov_buf, pairs);
}

/**
 * Retrieve a directory's layout by using an ioctl.
 *
 * @param fd        file descriptor of the directory we want the layout of
 *
 * @return          layout of the directory, or NULL
 */
static struct llapi_layout *
get_dir_data_striping(int fd)
{
    char tmp[XATTR_SIZE_MAX] = {0};
    struct llapi_layout *layout;
    struct lov_user_md *lum;
    int lum_size;
    int rc = 0;

    lum = (struct lov_user_md *)tmp;

    rc = ioctl(fd, LL_IOC_LOV_GETSTRIPE, (void *)lum);
    if (rc)
        return NULL;

    lum_size = lov_user_md_size(lum->lmm_stripe_count, lum->lmm_magic);
    layout = llapi_layout_get_by_xattr(lum, lum_size, 0);
    if (!layout)
        return NULL;

    return layout;
}

/**
 * Record a file's layout attributes:
 *  - main flags
 *  - magic number and layout generation if the file is regular
 *  - mirror_count if the file is composite
 *  - per component:
 *    - stripe_count
 *    - stripe_size
 *    - pattern
 *    - component flags
 *    - pool
 *    - ost
 *    - if the file is composite, 3 more attributes:
 *      - mirror_id
 *      - begin
 *      - end
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
static int
xattrs_get_layout(int fd, struct rbh_value_pair *pairs, int available_pairs)
{
    struct iterator_data data = { .comp_index = 0 };
    struct llapi_layout *layout;
    uint16_t mirror_count = 0;
    int required_pairs = 0;
    /**
     * There are 6 layout header components in total, but OST is in its own
     * list, so we only consider 5 attributes for the main data array allocation
     */
    int save_errno = 0;
    int nb_xattrs = 5;
    uint32_t nb_comp;
    int subcount = 0;
    uint32_t flags;
    int rc;

    if (S_ISLNK(mode))
        /* no layout to fetch for links */
        return 0;

    if (S_ISREG(mode))
        required_pairs = 3;
    else
        required_pairs = 1;

    if (available_pairs < required_pairs)
        return -1;

    if (S_ISDIR(mode)) {
        /* Directories have a default striping that children can inherit from.
         * These information can be manipulated as a regular file layout but
         * they are fetched differently through the Lustre API.
         */
        layout = get_dir_data_striping(fd);
        /* If layout is NULL and errno is ENODATA, that means the ioctl failed
         * because there is no default striping on the directory.
         */
        if (layout == NULL && errno == ENODATA)
            return 0;
    } else {
        layout = llapi_layout_get_by_fd(fd, 0);
    }

    if (layout == NULL)
        return -1;

    rc = llapi_layout_flags_get(layout, &flags);
    if (rc)
        goto err;

    rc = fill_uint32_pair("flags", flags, &pairs[subcount++], _values);
    if (rc)
        goto err;

    if (S_ISREG(mode)) {
        /* Magic number and generation are only meaningful for actual layouts,
         * not the default layout stored in the directory.
         */
        rc = xattrs_get_magic_and_gen(fd, &pairs[subcount]);
        if (rc < 0)
            goto err;

        subcount += rc;
    }

    available_pairs -= subcount;

    if (llapi_layout_is_composite(layout)) {
        if (available_pairs < 1)
            return -1;

        rc = llapi_layout_mirror_count_get(layout, &mirror_count);
        if (rc)
            goto err;

        rc = fill_uint32_pair("mirror_count", mirror_count, &pairs[subcount++],
                              _values);
        if (rc)
            goto err;

        nb_comp = 0;
        rc = llapi_layout_comp_iterate(layout, &layout_get_nb_comp, &nb_comp);
        if (rc)
            goto err;

        /** The file is composite, so we add 3 more xattrs to the main alloc */
        nb_xattrs += 3;
        available_pairs -= 1;
    } else {
        nb_comp = 1;
    }

    rc = init_iterator_data(&data, nb_comp, nb_xattrs);
    if (rc)
        goto err;

    if (llapi_layout_is_composite(layout)) {
        rc = llapi_layout_comp_iterate(layout, &xattrs_layout_iterator, &data);
    } else {
        rc = fill_iterator_data(layout, &data, 0);
        data.comp_index = 1;
    }

    if (rc)
        goto free_data;

    if (S_ISDIR(mode))
        required_pairs = nb_xattrs - 1;
    else
        required_pairs = nb_xattrs;

    if (available_pairs < required_pairs)
        return -1;

    rc = xattrs_fill_layout(&data, nb_xattrs, &pairs[subcount]);
    if (rc < 0)
        goto free_data;

    subcount += rc;
    rc = 0;

free_data:
    save_errno = errno;
    free(data.stripe_count);
    free(data.ost);

err:
    save_errno = save_errno ? : errno;
    llapi_layout_free(layout);
    errno = save_errno;
    return rc ? rc : subcount;
}

static int
xattrs_get_mdt_info(int fd, struct rbh_value_pair *pairs, int available_pairs)
{
    int required_pairs = 0;
    int subcount = 0;
    int rc = 0;

    if (S_ISDIR(mode))
        required_pairs = 5;
    else if (S_ISREG(mode))
        required_pairs = 1;

    if (available_pairs < required_pairs)
        return -1;

    if (S_ISDIR(mode)) {
        /* Fetch meta data striping for directories only */
        struct lmv_user_mds_data *objects;
        struct rbh_value *mdt_idx;
        struct lmv_user_md *lum;
        int stripe_count = 256;
        int save_errno = 0;

        /* This is how it is done in Lustre, initiate the lmv_user_md
         * structure to the correct magic number and a default stripe_count of
         * 256, do the ioctl which will fail because the stripe_count/size is
         * invalid, update the stripe_count, reallocate the structure, and do
         * the ioctl again.
         */
again:
        lum = calloc(1,
                     lmv_user_md_size(stripe_count, LMV_USER_MAGIC_SPECIFIC));
        lum->lum_magic = LMV_MAGIC_V1;
        lum->lum_stripe_count = stripe_count;

        rc = ioctl(fd, LL_IOC_LMV_GETSTRIPE, lum);
        if (rc) {
            if (errno == E2BIG)
                stripe_count = lum->lum_stripe_count;

            save_errno = errno;
            free(lum);
            errno = save_errno;

            switch (errno) {
            case E2BIG:
                goto again;
            case ENODATA:
                rc = fill_uint32_pair("mdt_count", 1,
                                      &pairs[subcount++], _values);
                if (rc)
                    return -1;

                goto get_mdt_idx;
            default:
                return rc;
            }
            __builtin_unreachable();
        }

        mdt_idx = calloc(lum->lum_stripe_count, sizeof(*mdt_idx));
        if (mdt_idx == NULL)
            goto free_lum;

        objects = lum->lum_objects;

        for (uint32_t i = 0; i < lum->lum_stripe_count; i++)
            mdt_idx[i] = create_uint32_value(objects[i].lum_mds);

        rc = fill_sequence_pair("child_mdt_idx", mdt_idx, lum->lum_stripe_count,
                                &pairs[subcount++], _values);
        save_errno = errno;
        free(mdt_idx);
        errno = save_errno;
        if (rc)
            goto free_lum;

        /* TODO: change this to "mdt_hash_type" when modifying the structure of
         * the Lustre attributes (i.e. "xattrs.mdt : { child_mdt_idx, hash_type,
         * hash_flags, count }")
         */
        rc = fill_uint32_pair("mdt_hash",
                              lum->lum_hash_type & LMV_HASH_TYPE_MASK,
                              &pairs[subcount++], _values);
        if (rc)
            goto free_lum;

        rc = fill_uint32_pair("mdt_hash_flags",
                              lum->lum_hash_type & ~LMV_HASH_TYPE_MASK,
                              &pairs[subcount++], _values);
        if (rc)
            goto free_lum;

        rc = fill_uint32_pair("mdt_count", lum->lum_stripe_count,
                              &pairs[subcount++], _values);

free_lum:
        save_errno = errno;
        free(lum);
        errno = save_errno;
        if (rc)
            return -1;
    }

get_mdt_idx:
    if (S_ISREG(mode) || S_ISDIR(mode)) {
        int32_t mdt;

        rc = llapi_file_fget_mdtidx(fd, &mdt);
        if (rc)
            return -1;

        rc = fill_int32_pair("mdt_index", mdt, &pairs[subcount++], _values);
        if (rc)
            return -1;
    }

    return subcount;
}

static int
_get_attrs(struct entry_info *entry_info,
           int (*attrs_funcs[])(int, struct rbh_value_pair *, int),
           int nb_attrs_funcs,
           struct rbh_value_pair *pairs,
           int available_pairs,
           struct rbh_sstack *values)
{
    int count = 0;
    int subcount;

    _inode_xattrs_count = entry_info->inode_xattrs_count;
    _inode_xattrs = entry_info->inode_xattrs;
    mode = entry_info->statx->stx_mode;
    _values = values;

    for (int i = 0; i < nb_attrs_funcs; ++i) {
        subcount = attrs_funcs[i](entry_info->fd, &pairs[count],
                                  available_pairs);
        if (subcount == -1)
            return -1;

        available_pairs -= subcount;
        count += subcount;
    }

    if (retention_enricher)
        count += retention_enricher(entry_info, 0, &pairs[count],
                                    available_pairs,
                                    values);

    return count;
}

static int
lustre_get_attrs(struct entry_info *entry_info,
                 struct rbh_value_pair *pairs,
                 int available_pairs,
                 struct rbh_sstack *values)
{
    int (*xattrs_funcs[])(int, struct rbh_value_pair *, int) = {
        xattrs_get_hsm, xattrs_get_layout, xattrs_get_mdt_info
    };

    return _get_attrs(entry_info, xattrs_funcs,
                      sizeof(xattrs_funcs) / sizeof(xattrs_funcs[0]),
                      pairs, available_pairs, values);
}

int
lustre_inode_xattrs_callback(struct entry_info *entry_info,
                             struct rbh_value_pair *pairs,
                             int available_pairs,
                             struct rbh_sstack *values)
{
    int (*xattrs_funcs[])(int, struct rbh_value_pair *, int) = {
        xattrs_get_fid, xattrs_get_hsm, xattrs_get_layout, xattrs_get_mdt_info
    };

    return _get_attrs(entry_info, xattrs_funcs,
                      sizeof(xattrs_funcs) / sizeof(xattrs_funcs[0]),
                      pairs, available_pairs, values);
}

/*----------------------------------------------------------------------------*
 |                          lustre_backend                                    |
 *----------------------------------------------------------------------------*/

static struct rbh_value *
lustre_get_default_dir_stripe(int fd, uint64_t flags)
{
    struct llapi_layout *layout;
    struct rbh_value *value;

    assert(flags & RBH_LEF_DIR_LOV);

    if (flags & ~(RBH_LEF_LUSTRE | RBH_LEF_ALL)) {
        /* Unsupported flag */
        errno = ENOTSUP;
        return NULL;
    }

    value = malloc(sizeof(*value));
    if (!value)
        return NULL;

    layout = get_dir_data_striping(fd);

    if (flags & RBH_LEF_STRIPE_COUNT) {
        if (!layout)
            value->uint64 = 0;
        else if (llapi_layout_stripe_count_get(layout, &value->uint64))
            return NULL;

        value->type = RBH_VT_UINT64;

    } else if (flags & RBH_LEF_STRIPE_SIZE) {
        if (!layout)
            value->uint64 = 0;
        else if (llapi_layout_stripe_size_get(layout, &value->uint64))
            return NULL;

        value->type = RBH_VT_UINT64;

    } else if (flags & RBH_LEF_STRIPE_PATTERN) {
        if (!layout)
            value->uint64 = 0;
        else if (llapi_layout_pattern_get(layout, &value->uint64))
            return NULL;

        value->type = RBH_VT_UINT64;
    }

    return value;
}

int
rbh_lustre_enrich(struct entry_info *einfo, uint64_t flags,
                  struct rbh_value_pair *pairs,
                  size_t pairs_count,
                  struct rbh_sstack *values)
{
    if (!rbh_attr_is_lustre(flags))
        /* No lustre flags to retrieve */
        return 0;

    if (flags == (RBH_LEF_LUSTRE | RBH_LEF_ALL_NOFID))
        return lustre_get_attrs(einfo, pairs, pairs_count, values);
    else if (flags == 0 || flags == (RBH_LEF_LUSTRE | RBH_LEF_ALL))
        return lustre_inode_xattrs_callback(einfo, pairs, pairs_count, values);

    if (flags & RBH_LEF_DIR_LOV) {
        pairs->value = lustre_get_default_dir_stripe(einfo->fd, flags);
        if (!pairs->value)
            return -1;

        return 1;
    }

    return 0;
}

    /*--------------------------------------------------------------------*
     |                          get_attribute()                           |
     *--------------------------------------------------------------------*/

int
lustre_get_attribute(uint64_t flags, void *_arg, struct rbh_value_pair *pairs,
                     int available_pairs)
{
    struct arg_t {
        int fd;
        struct rbh_statx *statx;
        struct rbh_sstack *values;
    };
    struct arg_t *info = (struct arg_t *)_arg;
    struct entry_info entry_info = {
        .fd = info->fd,
        .statx = info->statx,
        .inode_xattrs = NULL,
        .inode_xattrs_count = NULL,
    };

    return rbh_lustre_enrich(&entry_info, flags, pairs, available_pairs,
                             info->values);
}

static int
lustre_fts_backend_get_attribute(void *backend, uint64_t flags,
                                 void *arg, struct rbh_value_pair *pairs,
                                 int available_pairs)
{
    (void) backend;
    return lustre_get_attribute(flags, arg, pairs, available_pairs);
}

    /*--------------------------------------------------------------------*
     |                          get_option()                              |
     *--------------------------------------------------------------------*/

static int
lustre_fts_backend_get_option(void *backend, unsigned int option, void *data,
                              size_t *data_size)
{
    return posix_backend_get_option(backend, option, data, data_size);
}

    /*--------------------------------------------------------------------*
     |                          set_option()                              |
     *--------------------------------------------------------------------*/

static int
lustre_fts_backend_set_option(void *backend, unsigned int option,
                              const void *data, size_t data_size)
{
    return posix_backend_set_option(backend, option, data, data_size);
}

    /*--------------------------------------------------------------------*
     |                          branch()                                  |
     *--------------------------------------------------------------------*/

static struct rbh_backend *
lustre_fts_backend_branch(void *backend, const struct rbh_id *id,
                          const char *path)
{
    return posix_backend_branch(backend, id, path);
}

    /*--------------------------------------------------------------------*
     |                          root()                                    |
     *--------------------------------------------------------------------*/

static struct rbh_fsentry *
lustre_fts_backend_root(void *backend,
                        const struct rbh_filter_projection *projection)
{
    return posix_root(backend, projection);
}

    /*--------------------------------------------------------------------*
     |                          filter()                                  |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
lustre_fts_backend_filter(void *backend, const struct rbh_filter *filter,
                          const struct rbh_filter_options *options,
                          const struct rbh_filter_output *output)
{
    return posix_backend_filter(backend, filter, options, output);
}

    /*--------------------------------------------------------------------*
     |                          destroy()                                 |
     *--------------------------------------------------------------------*/

static void
lustre_fts_backend_destroy(void *backend)
{
    posix_backend_destroy(backend);
}

struct rbh_mut_iterator *
lustre_iterator_new(const char *root, const char *entry, int statx_sync_type)
{
    struct posix_iterator *lustre_iter;

    lustre_iter = (struct posix_iterator *)
                   posix_iterator_new(root, entry, statx_sync_type);
    if (lustre_iter == NULL)
        return NULL;

    lustre_iter->inode_xattrs_callback = lustre_inode_xattrs_callback;

    return (struct rbh_mut_iterator *)lustre_iter;
}

static const struct rbh_backend_operations LUSTRE_BACKEND_OPS = {
    .get_option = lustre_fts_backend_get_option,
    .set_option = lustre_fts_backend_set_option,
    .branch = lustre_fts_backend_branch,
    .root = lustre_fts_backend_root,
    .filter = lustre_fts_backend_filter,
    .get_attribute = lustre_fts_backend_get_attribute,
    .destroy = lustre_fts_backend_destroy,
};

struct rbh_backend *
rbh_lustre_backend_new(const struct rbh_backend_plugin *self,
                       const char *type,
                       const char *path,
                       struct rbh_config *config)
{
    struct posix_backend *lustre;

    lustre = (struct posix_backend *) rbh_posix_backend_new(self, NULL, path,
                                                            config);
    if (lustre == NULL)
        return NULL;

    lustre->iter_new = lustre_iterator_new;
    lustre->backend.id = RBH_BI_LUSTRE;
    lustre->backend.name = RBH_LUSTRE_BACKEND_NAME;
    lustre->backend.ops = &LUSTRE_BACKEND_OPS;

    load_rbh_config(config);

    if (self) {
        const struct rbh_posix_extension *retention;

        /* For backward compatibility, Lustre explicitly loads the retention
         * extension. This will be removed later.
         */
        retention = rbh_posix_load_extension(&self->plugin, "retention");
        if (retention) {
            if (retention->setup_enricher)
                retention->setup_enricher();

            retention_enricher = retention->enrich;
        }
    }

    return &lustre->backend;
}
