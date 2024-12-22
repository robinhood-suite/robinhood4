/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_LUSTRE_H
#define RBH_LUSTRE_H

#include "robinhood/sstack.h"

#include "common.h"

/*----------------------------------------------------------------------------*
 |                             lustre_operations                              |
 *----------------------------------------------------------------------------*/

struct rbh_mut_iterator *
lustre_iterator_new(const char *root, const char *entry, int statx_sync_type);

int
lustre_inode_xattrs_callback(struct entry_info *entry_info,
                             struct rbh_value_pair *pairs,
                             int available_pairs,
                             struct rbh_sstack *values);

int
lustre_get_attribute(const char *attr_name, void *_arg,
                     struct rbh_value_pair *pairs, int available_pairs);

#endif
