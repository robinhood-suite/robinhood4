/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_OPEN_H
#define ROBINHOOD_OPEN_H

#include "robinhood/id.h"

int
mount_fd_by_root(const char *root);

int
open_by_id(int mount_fd, const struct rbh_id *id, int flags);

int
open_by_id_generic(int mount_fd, const struct rbh_id *id);

int
open_by_id_opath(int mount_fd, const struct rbh_id *id);

/* This function handles open_by_id when there is no need to fully open the
 * file. For instance this function can be used with statx.
 */

#endif
