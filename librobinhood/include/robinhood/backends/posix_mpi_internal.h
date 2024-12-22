/* This file is part of RobinHood 4
 *
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_POSIX_MPI_H
#define RBH_POSIX_MPI_H

/*----------------------------------------------------------------------------*
 |                        posix_mpi operations                                |
 *----------------------------------------------------------------------------*/

#include "robinhood/id.h"

struct rbh_backend *
posix_mpi_backend_branch(void *backend, const struct rbh_id *id,
                         const char *path);

#endif
