/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_LUSTRE_H
#define RBH_LUSTRE_H

#include "robinhood/backend.h"
#include "robinhood/sstack.h"

/*----------------------------------------------------------------------------*
 |                             lustre_operations                              |
 *----------------------------------------------------------------------------*/

int
lustre_inode_xattrs_callback(const int fd, const struct rbh_statx *statx,
                             struct rbh_value_pair *inode_xattrs,
                             ssize_t *inode_xattrs_count,
                             struct rbh_value_pair *pairs,
                             struct rbh_sstack *values);

int
lustre_get_attribute(const char *attr_name, void *_arg,
                     struct rbh_value_pair *data);

#endif
