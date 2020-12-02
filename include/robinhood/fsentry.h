/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_FSENTRY_H
#define ROBINHOOD_FSENTRY_H

#include <robinhood/id.h>
#include <robinhood/value.h>

/** @file
 * fsentry is the generic name for a filesystem entry (file, dir, symlink, ...)
 */

struct statx;

/**
 * Any filesystem entry (file, dir, symlink, ...)
 *
 * \c mask is set to indicate which fields of an fsentry are set.
 *
 * To access the content of \c statx, either use the libc definition if it
 * provides one or include <robinhood/statx.h>.
 */
struct rbh_fsentry {
    /**
     * Mask of bits indicating filled fields
     *
     * To be compared with values from enum rbh_fsentry_property.
     */
    unsigned int mask;
    /**
     * Unique identifier for the fsentry
     *
     * While not mandated by design, this ID is expected to have a very specific
     * format which allows it to be converted to/from a struct file_handle.
     *
     * cf. rbh_id_from_file_handle()
     */
    struct rbh_id id;
    /**
     * ID of the parent fsentry (for a given link in the namespace)
     *
     * There may be several fsentries with the same ID but with different
     * parents (and/or names), both because of hardlinks, and the eventual
     * consistency of robinhood backends.
     */
    struct rbh_id parent_id;
    /**
     * Name of fsentry (for a given link in the namespace)
     *
     * There may be several fsentries with the same ID but with different names
     * (and/or parents), both because of hardlinks, and the eventual consistency
     * of robinhood backends.
     */
    const char *name;
    /**
     * statx attributes for the fsentry
     */
    const struct statx *statx;
    /* Extended attributes
     *
     * An extended attribute in librobinhood is similar to a regular filesystem
     * xattr, it is a key/value pair, where the key is necessarily a string and
     * the value can be any valid struct rbh_value.
     *
     * xattrs can be used to both store information present on the original
     * filesystem, as well as user-defined enrichments.
     */
    struct {
        /**
         * Namespace extended attributes
         *
         * Much like regular xattrs in filesystems, except they are attached to
         * a **namespace** entry rather than an inode.
         */
        struct rbh_value_map ns; /* namespace is a reserved keyword in C++ */
        /**
         * Namespace extended attributes
         *
         * Much like regular xattrs in filesystems.
         */
        struct rbh_value_map inode;
    } xattrs;
    /**
     * Content of a symlink
     */
    char symlink[];
};

/**
 * Bits used to designate fields of a struct rbh_fsentry
 */
enum rbh_fsentry_property {
    RBH_FP_ID               = 0x0001,
    RBH_FP_PARENT_ID        = 0x0002,
    RBH_FP_NAME             = 0x0004,
    RBH_FP_STATX            = 0x0008,
    RBH_FP_SYMLINK          = 0x0010,
    RBH_FP_NAMESPACE_XATTRS = 0x0020,
    RBH_FP_INODE_XATTRS     = 0x0040,
};
#define RBH_FP_ALL            0x007f

/**
 * Create an fsentry in a single memory allocation
 *
 * @param id            id of the fsentry to create
 * @param parent_id     parent id of fsentry to create
 * @param name          name of the fsentry
 * @param statx         pointer to the struct statx of the fsentry to create
 * @param ns_xattrs     namespace xattrs
 * @param xattrs        inode xattrs
 * @param symlink       the content of the symlink
 *
 * @return              a pointer to a newly allocated struct rbh_fsentry on
 *                      success, NULL on error and errno is set appropriately
 *
 * @error ENOMEM        there was not enough memory available
 * @error EINVAL        \p symlink was provided, but the field \c stx_mode of
 *                      \p statx is not that of a symlink
 *
 * The returned fsentry can be freed with a single call to free().
 * The returned fsentry points at internally managed copies of \p id,
 * \p parent_id, \p name, ...
 *
 * Any of \p id, \p parent_id, \p name, \p statx, \p ns_xattrs and \p xattrs may
 * be NULL, in which case the corresponding fields in the returned fsentry are
 * not set.
 */
struct rbh_fsentry *
rbh_fsentry_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                const char *name, const struct statx *statx,
                const struct rbh_value_map *ns_xattrs,
                const struct rbh_value_map *xattrs, const char *symlink);

#endif
