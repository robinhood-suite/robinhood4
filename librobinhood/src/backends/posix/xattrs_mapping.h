/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_POSIX_XATTRS_MAPPING_H
#define RBH_POSIX_XATTRS_MAPPING_H

#include <sys/types.h>

#include "robinhood/sstack.h"
#include "robinhood/value.h"

struct rbh_value *
create_value_from_xattr(const char *name, const char *buffer, ssize_t length,
                        struct rbh_sstack *xattrs);

int
set_xattrs_types_map();

#endif
