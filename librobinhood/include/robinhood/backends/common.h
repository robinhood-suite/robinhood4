/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_COMMON_H
#define RBH_COMMON_H

#include <sys/types.h> /* ssize_t */

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

#endif
