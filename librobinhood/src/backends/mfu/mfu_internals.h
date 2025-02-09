/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_MFU_INTERNALS_H
#define ROBINHOOD_MFU_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <mfu.h>
#include <stddef.h>
#include <stdint.h>

#include <robinhood/backends/mfu.h>
#include <robinhood/backends/posix_extension.h>

/*----------------------------------------------------------------------------*
 |                          POSIX iterator interface                          |
 *----------------------------------------------------------------------------*/

struct rbh_mut_iterator *
rbh_posix_mfu_iter_new(const char *root,
                       const char *entry,
                       int statx_sync_type);

/*----------------------------------------------------------------------------*
 |                             Utility functions                              |
 *----------------------------------------------------------------------------*/

mfu_flist
walk_path(const char* path);

struct rbh_id *
get_parent_id(const char *path, bool use_fd, int prefix_len, short backend_id);

struct rbh_fsentry *
fsentry_from_fi(struct file_info *fi, struct posix_iterator *posix);

#endif
