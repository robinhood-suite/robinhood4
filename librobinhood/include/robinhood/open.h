/* This file is part of the RobinHood Library
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
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

#endif
