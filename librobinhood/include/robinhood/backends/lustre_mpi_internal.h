/* This file is part of RobinHood 4
 *
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_LUSTRE_MPI_H
#define RBH_LUSTRE_MPI_H

/**
 * @file
 *
 * Internal header which defines the mpi_iterator structure
 *
 */

#include "mfu.h"
#include "robinhood/iterator.h"
#include "robinhood/sstack.h"
#include "robinhood/value.h"

/*----------------------------------------------------------------------------*
 |                                mpi_iterator                                |
 *----------------------------------------------------------------------------*/

struct mpi_iterator {
    struct rbh_mut_iterator iterator;

    /**
     * Callback for managing and filling namespace xattrs
     *
     * @param fd        file descriptor of the entry
     * @param mode      mode of file examined
     * @param pairs     list of rbh_value_pairs to fill
     * @param values    stack that will contain every rbh_value of
     *                  \p pairs
     *
     * @return          number of filled \p pairs
     */
    int (*ns_xattrs_callback)(const int fd, const uint16_t mode,
                              struct rbh_value_pair *inode_xattrs,
                              ssize_t *inode_xattrs_count,
                              struct rbh_value_pair *pairs,
                              struct rbh_sstack *values);

    int statx_sync_type;
    size_t prefix_len;
    bool skip_error;
    uint64_t current;
    uint64_t total;
    mfu_flist flist;
};

#endif
