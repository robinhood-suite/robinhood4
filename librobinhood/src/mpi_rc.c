/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "robinhood/mpi_rc.h"

#include <stdatomic.h>
#include <pthread.h>
#include <mpi.h>

static int (*custom_finalize_cb)(void) = NULL;
static int (*custom_initialize_cb)(void) = NULL;
static pthread_mutex_t mpi_lock = PTHREAD_MUTEX_INITIALIZER;
static int mpi_rc;

void
rbh_set_custom_initialize(int (*custom_initialize)(void))
{
    custom_initialize_cb = custom_initialize;
}

void
rbh_set_custom_finalize(int (*custom_finalize)(void))
{
    custom_finalize_cb = custom_finalize;
}

void
rbh_mpi_initialize(void)
{
    int initialized;

    MPI_Initialized(&initialized);
    if (!initialized) {
        MPI_Init(NULL, NULL);
        if (custom_initialize_cb)
            custom_initialize_cb();
    }
}

void
rbh_mpi_finalize(void)
{
    int initialized;
    int finalized;

    /* Prevents finalizing twice MPI if we use two backends with MPI */
    MPI_Initialized(&initialized);
    MPI_Finalized(&finalized);

    if (initialized && !finalized) {
        if (custom_finalize_cb)
            custom_finalize_cb();
        MPI_Finalize();
    }
}

void
rbh_mpi_inc_ref(void (*init)(void))
{
    pthread_mutex_lock(&mpi_lock);
    mpi_rc++;
    if (mpi_rc == 1)
        init();

    pthread_mutex_unlock(&mpi_lock);
}

void
rbh_mpi_dec_ref(void (*fini)(void))
{
    pthread_mutex_lock(&mpi_lock);
    mpi_rc--;
    if (mpi_rc == 0)
        fini();

    pthread_mutex_unlock(&mpi_lock);
}
