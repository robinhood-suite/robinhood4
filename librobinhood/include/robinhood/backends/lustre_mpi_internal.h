/* This file is part of RobinHood 4
 *
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_LUSTRE_MPI_H
#define RBH_LUSTRE_MPI_H

/*----------------------------------------------------------------------------*
 |                       lustre_mpi operations                                |
 *----------------------------------------------------------------------------*/

struct rbh_backend *
lustre_mpi_backend_branch(void *backend, const struct rbh_id *id,
                          const char *path);

#endif
