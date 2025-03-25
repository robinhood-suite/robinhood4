/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
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

#include <fts.h>
#include <unistd.h>

#include "robinhood/backend.h"
#include "robinhood/sstack.h"

#include "common.h"

/*----------------------------------------------------------------------------*
 |                               posix_iterator                               |
 *----------------------------------------------------------------------------*/

struct posix_iterator {
    struct rbh_mut_iterator iterator;
    inode_xattrs_callback_t inode_xattrs_callback;
    int statx_sync_type;
    size_t prefix_len;
    FTS *fts_handle;
    FTSENT *ftsent;
    bool skip_error;
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

struct rbh_id *
id_from_fd(int fd, short backend_id);

char *
freadlink(int fd, const char *path, size_t *size_);

bool
fsentry_from_any(struct fsentry_id_pair *fip, const struct rbh_value *path,
                 char *accpath, struct rbh_id *entry_id,
                 struct rbh_id *parent_id, char *name, int statx_sync_type,
                 inode_xattrs_callback_t inode_xattrs_callback);

/*----------------------------------------------------------------------------*
 |                              posix_operations                              |
 *----------------------------------------------------------------------------*/

int
posix_backend_get_option(void *backend, unsigned int option, void *data,
                         size_t *data_size);
int
posix_backend_set_option(void *backend, unsigned int option, const void *data,
                         size_t data_size);

struct posix_branch_backend *
posix_create_backend_branch(void *backend, const struct rbh_id *id,
                            const char *path);

struct rbh_backend *
posix_backend_branch(void *backend, const struct rbh_id *id, const char *path);

struct rbh_fsentry *
posix_root(void *backend, const struct rbh_filter_projection *projection);

struct rbh_mut_iterator *
posix_backend_filter(void *backend, const struct rbh_filter *filter,
                     const struct rbh_filter_options *options,
                     const struct rbh_filter_output *output);

void
posix_backend_destroy(void *backend);

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
};

struct posix_branch_backend {
    struct posix_backend posix;
    struct rbh_id id;
    char *path;
};

#endif
