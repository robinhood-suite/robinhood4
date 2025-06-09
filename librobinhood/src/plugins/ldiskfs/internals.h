/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_LDISKFS_INTERNALS_H
#define ROBINHOOD_LDISKFS_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/backends/ldiskfs.h>
#include <robinhood/utils.h>
#include <ext2fs/ext2fs.h>

#include "dcache.h"

struct ldiskfs_backend {
    struct rbh_backend backend;
    ext2_filsys fs;
    ext2_inode_scan iscan;
    struct rbh_dcache *dcache;
};

void
ldiskfs_backend_destroy(void *backend);

bool
scan_target(struct ldiskfs_backend *backend);

#endif
