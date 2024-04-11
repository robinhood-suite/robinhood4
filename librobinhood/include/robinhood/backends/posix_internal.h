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

#include <fts.h>

#include "robinhood/backend.h"
#include "robinhood/sstack.h"

/*----------------------------------------------------------------------------*
 |                               posix_iterator                               |
 *----------------------------------------------------------------------------*/

/**
 * This structure corresponds to the information available about an entry for
 * other backends.
 */
struct entry_info {
    int fd;                              /* The file descriptor of the entry */
    struct rbh_statx *statx;             /* The statx of the entry */
    struct rbh_value_pair *inode_xattrs; /* The inode xattrs of the entry */
    ssize_t *inode_xattrs_count;         /* The number of inode xattrs */
};

struct posix_iterator {
    struct rbh_mut_iterator iterator;

    /**
     * Callback for managing and filling inode xattrs
     *
     * @param info                 Information about an entry
     * @param pairs                list of rbh_value_pairs to fill
     * @param available_pairs      the number of pairs that can be filled
     * @param values               stack that will contain every rbh_value of
     *                             \p pairs
     *
     * @return                     number of filled \p pairs
     */
    int (*inode_xattrs_callback)(struct entry_info *info,
                                 struct rbh_value_pair *pairs,
                                 int available_pairs,
                                 struct rbh_sstack *values);

    int statx_sync_type;
    size_t prefix_len;
    FTS *fts_handle;
    FTSENT *ftsent;
    bool skip_error;
};

struct posix_iterator *
posix_iterator_new(const char *root, const char *entry, int statx_sync_type);

/*----------------------------------------------------------------------------*
 |                              posix_operations                              |
 *----------------------------------------------------------------------------*/

int
posix_backend_get_option(void *backend, unsigned int option, void *data,
                         size_t *data_size);
int
posix_backend_set_option(void *backend, unsigned int option, const void *data,
                         size_t data_size);

struct rbh_backend *
posix_backend_branch(void *backend, const struct rbh_id *id, const char *path);

struct rbh_fsentry *
posix_root(void *backend, const struct rbh_filter_projection *projection);

struct rbh_mut_iterator *
posix_backend_filter(void *backend, const struct rbh_filter *filter,
                     const struct rbh_filter_options *options);

void
posix_backend_destroy(void *backend);

/*----------------------------------------------------------------------------*
 |                               posix_backend                                |
 *----------------------------------------------------------------------------*/

struct posix_backend {
    struct rbh_backend backend;
    struct posix_iterator *(*iter_new)(const char *, const char *, int);
    char *root;
    int statx_sync_type;
};

#endif
