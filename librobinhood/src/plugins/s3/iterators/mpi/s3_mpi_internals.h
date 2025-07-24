/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_S3_MPI_INTERNALS_H
#define ROBINHOOD_S3_MPI_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>

#include <robinhood/backends/s3_mpi.h>
#include <robinhood/backends/s3_extension.h>

/*----------------------------------------------------------------------------*
 |                             S3 iterator interface                          |
 *----------------------------------------------------------------------------*/

struct s3_iterator *
rbh_s3_mpi_iter_new();

#endif

