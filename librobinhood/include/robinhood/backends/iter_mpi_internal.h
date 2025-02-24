/* This file is part of RobinHood 4
 *
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_ITER_MPI_H
#define RBH_ITER_MPI_H

/**
 * @file
 *
 * Internal header which defines the mpi_iterator
 *
 */

#include "mfu.h"

#include "robinhood/backend.h"
#include "robinhood/sstack.h"

#include "common.h"

/*----------------------------------------------------------------------------*
 |                               mpi_file_info                                |
 *----------------------------------------------------------------------------*/

struct mpi_file_info {
    /**
     * File path from the root
     */
    const char *path;

    /**
     * File name
     */
    char *name;

    /**
     * rbh_id of parent entry
     */
    struct rbh_id *parent_id;
};

/*----------------------------------------------------------------------------*
 |                                mpi_iterator                                |
 *----------------------------------------------------------------------------*/

struct mpi_iterator {
    struct rbh_mut_iterator iterator;
    inode_xattrs_callback_t inode_xattrs_callback;
    enum rbh_backend_id backend_id;
    int statx_sync_type;
    size_t prefix_len;

    /**
     * Operation used to build a new fsentry from mpifileutils
     */
    struct rbh_fsentry *(*mpi_build_fsentry)(struct mpi_file_info *mpi_fi,
                                             struct mpi_iterator *iterator);
    /**
     * Boolean to indicate if we build the rbh_id with a file descriptor or
     * a path
     */
    bool use_fd;

    /**
     * Boolean to indicate if we want to skip errors while synchronizing two
     * backends
     */
    bool skip_error;

    /**
     * Boolean to indicate if we are synchronizing a branch
     */
    bool is_branch;

    /**
     * Index of current file in the flist in the process
     */
    uint64_t current;

    /**
     * Size of the flist in the process,
     * this is not the global size of the flist
     */
    uint64_t total;
    mfu_flist flist;
};

/*----------------------------------------------------------------------------*
 |                       mpi_iterator operations                              |
 *----------------------------------------------------------------------------*/

struct rbh_id *
get_parent_id(const char *path, bool use_fd, int prefix_len, short backend_id);

struct rbh_mut_iterator *
mpi_iterator_new(const char *root, const char *entry, int statx_sync_type);

void *
mpi_iter_next(void *iterator);

/*----------------------------------------------------------------------------*
 |                       mpi_backend operations                               |
 *----------------------------------------------------------------------------*/

struct rbh_mut_iterator *
mpi_backend_filter(void *backend, const struct rbh_filter *filter,
                   const struct rbh_filter_options *options);

struct rbh_mut_iterator *
mpi_branch_backend_filter(void *backend, const struct rbh_filter *filter,
                          const struct rbh_filter_options *options);

/**
 * Release the memory associated with the plugin
 */
void
rbh_mpi_plugin_destroy();

#endif
