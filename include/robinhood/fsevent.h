/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_FSEVENT_H
#define ROBINHOOD_FSEVENT_H

#include "robinhood/fsentry.h"

enum rbh_fsevent_type {
    RBH_FET_UPSERT,
    RBH_FET_LINK,
    RBH_FET_UNLINK,
    RBH_FET_DELETE,
    RBH_FET_XATTR,
};

struct statx;

struct rbh_fsevent {
    enum rbh_fsevent_type type;
    struct rbh_id id;
    /* To unset an xattr, use a key/value pair with value set to NULL */
    struct rbh_value_map xattrs;
    union {
        /* RBH_FET_UPSERT */
        struct {
            const struct statx *statx; /* Nullable */
            const char *symlink; /* Nullable */
        } upsert;

        /* RBH_FET_LINK / RBH_FET_UNLINK / RBH_FET_XATTR */
        struct {
            /* If `type' is RBH_FET_LINK or RBH_FET_UNLINK, neither
             * `parent_id' nor `name' shall be NULL.
             *
             * Otherwise, if `type' is RBH_FET_XATTR, either:
             *     parent_id == NULL && name == NULL
             *
             *     => the `xattrs.inode' field of every fsentry whose ID matches
             *        (ie. every hardlink) is updated
             * Or:
             *     parent_id != NULL && name != NULL
             *
             *     => the `xattrs.ns' field of the (only) fsentry whose ID,
             *        parent ID, and name matches is updated
             */
            const struct rbh_id *parent_id;
            const char *name;
        } link, ns;
    };
};

struct rbh_fsevent *
rbh_fsevent_upsert_new(const struct rbh_id *id,
                       const struct rbh_value_map *xattrs,
                       const struct statx *statxbuf, const char *symlink);

struct rbh_fsevent *
rbh_fsevent_link_new(const struct rbh_id *id,
                     const struct rbh_value_map *xattrs,
                     const struct rbh_id *parent_id, const char *name);

struct rbh_fsevent *
rbh_fsevent_unlink_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                       const char *name);

struct rbh_fsevent *
rbh_fsevent_delete_new(const struct rbh_id *id);

struct rbh_fsevent *
rbh_fsevent_xattr_new(const struct rbh_id *id,
                      const struct rbh_value_map *xattrs);

struct rbh_fsevent *
rbh_fsevent_ns_xattr_new(const struct rbh_id *id,
                         const struct rbh_value_map *xattrs,
                         const struct rbh_id *parent_id, const char *name);

#endif
