/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_COMMON_H
#define RBH_COMMON_H

#include <sys/types.h> /* ssize_t */
#include "robinhood/sstack.h"

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
typedef int (*inode_xattrs_callback_t)(struct entry_info *info,
                                       struct rbh_value_pair *pairs,
                                       int available_pairs,
                                       struct rbh_sstack *values);

#endif
