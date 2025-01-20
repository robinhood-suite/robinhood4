/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_MPI_H
#define ROBINHOOD_MPI_H

/**
 * @file
 *
 * These functions provide a way to atomically setup/cleanup MPI across plugins.
 * For instance, if two backends use MPI, they will both call
 * MPI_Init/MPI_Finalize at some point. We need to make sure that MPI_Finalize
 * is only called once all the backends have finished using MPI based functions.
 *
 * Since the code of plugins is independant, we can't easily share state across
 * module boundaries. So the simplest solution is to have this shared state
 * inside librobinhood (e.g. mpi_rc and mpi_lock in mpi_rc.c). The actual
 * initialization/cleanup will be performed in the callbacks provided by the
 * plugins as parameters. The goal is to avoid dependencies to MPI inside
 * librobinhood to keep things modular.
 */

/**
 * Atomically increment the number of MPI references.
 * Call \p init on the first increment.
 */
void
rbh_mpi_inc_ref(void (*init)(void));

/**
 * Atomically decrement the number of MPI references.
 * Call \p fini when the ref count drops to 0.
 */
void
rbh_mpi_dec_ref(void (*fini)(void));

#endif
