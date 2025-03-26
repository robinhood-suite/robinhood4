/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_FSEVENT_H
#define ROBINHOOD_FSEVENT_H

#include "robinhood/fsentry.h"

/**
 * Types of fsevent
 */
enum rbh_fsevent_type {
    /**
     * Create/Update an inode in the backend
     *
     * If the inode does not exist, it will be created.
     *
     * If it exists, it will simply be updated.
     */
    RBH_FET_UPSERT,
    /**
     * Associate a couple (parent, name) to an inode
     *
     * This association is called a link.
     *
     * If the inode does not already exist, it will be created.
     *
     * One can initialize namespace extended attribute with this event.
     *
     * If the link already exists, any extended attribute (ie. namespace
     * extended attribute) previously associated with it will be lost.
     */
    RBH_FET_LINK,
    /** Delete a link from the backend */
    RBH_FET_UNLINK,
    /** Delete an inode and all the links that point at it from the backend */
    RBH_FET_DELETE,
    /** Update either an inode's or a link's extended attributes */
    RBH_FET_XATTR,
};

struct rbh_statx;

struct rbh_fsevent {
    /** Type of the fsevent */
    enum rbh_fsevent_type type;
    /** An fsentry's ID */
    struct rbh_id id;
    /**
     * Extended attributes to set/unset
     *
     * To unset an xattr, use a key/value pair with value set to NULL.
     */
    struct rbh_value_map xattrs;
    union {
        /** `type' == RBH_FET_UPSERT */
        struct {
            const struct rbh_statx *statx; /* Nullable */
            const char *symlink; /* Nullable */
        } upsert;

        /**
         * `type' == RBH_FET_LINK / RBH_FET_UNLINK / RBH_FET_XATTR
         *
         * If `type' is RBH_FET_LINK or RBH_FET_UNLINK, neither `parent_id'
         * nor `name' shall be NULL.
         *
         * Otherwise, if `type' is RBH_FET_XATTR, either:
         *
         *     parent_id == NULL && name == NULL
         *
         * => the `xattrs.inode' field of every fsentry whose ID matches
         *    (ie. every hardlink) is updated
         *
         * Or:
         *
         *     parent_id != NULL && name != NULL
         *
         * => the `xattrs.ns' field of the (only) fsentry whose ID,
         *    parent ID, and name matches is updated
         */
        struct {
            /** The fsentry's parent ID */
            const struct rbh_id *parent_id;
            /** The fsentry's name */
            const char *name;
        } link, ns;
    };
};

/**
 * Create an upsert fsevent
 *
 * @param id        the ID of the fsentry to upsert
 * @param xattrs    the xattrs associated with the fsentry to upsert
 * @param statxbuf  the inode attributes of the fsentry to upsert
 * @param symlink   the content of the symlink (only if \p ID refers to a
 *                  symlink)
 *
 * @return          a pointer to a newly allocated upsert fsevent on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p statxbuf says \p ID is not a symlink, but \p symlink is
 *                  not NULL
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_fsevent *
rbh_fsevent_upsert_new(const struct rbh_id *id,
                       const struct rbh_value_map *xattrs,
                       const struct rbh_statx *statxbuf, const char *symlink);

/**
 * Create a link fsevent
 *
 * @param id        the ID of the fsentry to link into the namespace
 * @param xattrs    the namespace xattrs associated with the link to create
 * @param parent_id the parent ID of the link to create
 * @param name      the name of the link to create
 *
 * @return          a pointer to a newly allocated link fsevent on success, NULL
 *                  on error and errno is set appropriately
 *
 * @error EINVAL    \p parent_id and/or \p name were NULL
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_fsevent *
rbh_fsevent_link_new(const struct rbh_id *id,
                     const struct rbh_value_map *xattrs,
                     const struct rbh_id *parent_id, const char *name);

/**
 * Create an unlink fsevent
 *
 * @param id        the ID of the fsentry to unlink from the namespace
 * @param parent_id the parent ID of the link to delete
 * @param name      the name of the link to delete
 *
 * @return          a pointer to a newly allocated unlink fsevent on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p parent_id and/or \p name were NULL
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_fsevent *
rbh_fsevent_unlink_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                       const char *name);

/**
 * Create a delete fsevent
 *
 * @param id        the ID of the fsentry to delete
 *
 * @return          a pointer to a newly allocated delete fsevent on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_fsevent *
rbh_fsevent_delete_new(const struct rbh_id *id);

/**
 * Create an inode xattr fsevent
 *
 * @param id        the ID of the fsentry to update
 * @param xattrs    the inode xattrs to update
 *
 * @return          a pointer to a newly allocated inode xattr fsevent on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_fsevent *
rbh_fsevent_xattr_new(const struct rbh_id *id,
                      const struct rbh_value_map *xattrs);

/**
 * Create a namespace xattr fsevent
 *
 * @param id        the ID of the fsentry to update
 * @param xattrs    the namespace xattrs to update
 * @param parent_id the parent ID of the link to target
 * @param name      the name of the link to target
 *
 * @return          a pointer to a newly allocated unlink fsevent on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p parent_id and/or \p name were NULL
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_fsevent *
rbh_fsevent_ns_xattr_new(const struct rbh_id *id,
                         const struct rbh_value_map *xattrs,
                         const struct rbh_id *parent_id, const char *name);

/**
 * Return the path xattr of \p fsevent
 *
 * @return          return the value associated with the path xattr if found,
 *                  NULL otherwise
 *
 * @error ENODATA   if the path was not found
 * @error EFAULT    if the path was found but is not a string
 */
const char *
rbh_fsevent_path(const struct rbh_fsevent *fsevent);

#endif
