/* This file is part of RobinHood 4
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_POSIX_H
#define RBH_POSIX_H

/**
 * @file
 *
 * Internal header which defines the posix_iterator and posix_backend
 * structures, used by each backend which overloads the posix one.
 *
 * For now, the only available overload is through ns_xattrs_callback
 * posix_iterator field, which may add extended attributes to the namespace.
 */

#include <unistd.h>

#include "robinhood/backend.h"
#include "robinhood/backends/posix_extension.h"

#include "common.h"

/*----------------------------------------------------------------------------*
 |                               posix_iterator                               |
 *----------------------------------------------------------------------------*/

struct posix_iterator {
    struct rbh_mut_iterator iterator;
    inode_xattrs_callback_t inode_xattrs_callback;
    enricher_t *enrichers;
    int statx_sync_type;
    size_t prefix_len;
    bool skip_error;
    char *path;
};

/**
 * Structure containing the result of fsentry_from_any
 */
struct fsentry_id_pair {
    /**
     * rbh_id of the rbh_fsentry,
     * used by the POSIX backend, corresponds to the fsentry's parent
     */
    struct rbh_id *id;
    struct rbh_fsentry *fsentry;
};

struct rbh_mut_iterator *
posix_iterator_new(const char *root, const char *entry, int statx_sync_type);

struct rbh_mut_iterator *
fts_iter_new(const char *root, const char *entry, int statx_sync_type);

int
fts_iter_root_setup(struct posix_iterator *_iter);

bool
rbh_posix_iter_is_fts(struct posix_iterator *iter);

struct rbh_id *
id_from_fd(int fd, short backend_id);

char *
freadlink(int fd, const char *path, size_t *size_);

bool
fsentry_from_any(struct fsentry_id_pair *fip, const struct rbh_value *path,
                 char *accpath, struct rbh_id *entry_id,
                 struct rbh_id *parent_id, char *name, int statx_sync_type,
                 inode_xattrs_callback_t inode_xattrs_callback,
                 enricher_t *enrichers);

char *
id2path(const char *root, const struct rbh_id *id);

/*----------------------------------------------------------------------------*
 |                               posix_backend                                |
 *----------------------------------------------------------------------------*/

struct posix_backend {
    struct rbh_backend backend;
    struct rbh_mut_iterator *(*iter_new)(const char *, const char *, int);
    char *root;
    int statx_sync_type;
    enricher_t *enrichers;
};

struct posix_branch_backend {
    struct posix_backend posix;
    struct rbh_id id;
    char *path;
};

#endif
