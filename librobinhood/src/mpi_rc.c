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

static pthread_mutex_t mpi_lock = PTHREAD_MUTEX_INITIALIZER;
static int mpi_rc;

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
