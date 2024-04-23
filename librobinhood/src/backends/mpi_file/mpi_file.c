/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>
#include <errno.h>
#include <string.h>

#include "mfu.h"

#include "robinhood/backends/mpi_file.h"

/*----------------------------------------------------------------------------*
 |                          mpi_file_backend                                  |
 *----------------------------------------------------------------------------*/

struct mpi_file_backend {
    struct rbh_backend backend;
    /*
     * Name of the mpi-file
     */
    const char *name;
    mfu_flist flist;
};

    /*--------------------------------------------------------------------*
     |                          destroy()                                 |
     *--------------------------------------------------------------------*/

static void
mpi_file_backend_destroy(void *backend)
{
    struct mpi_file_backend *mpi_file = backend;

    mfu_flist_free(&mpi_file->flist);
    free(backend);
}

    /*--------------------------------------------------------------------*
     |                          backend()                                 |
     *--------------------------------------------------------------------*/

static const struct rbh_backend_operations MPI_FILE_BACKEND_OPS = {
    .destroy = mpi_file_backend_destroy,
};

static const struct rbh_backend MPI_FILE_BACKEND = {
    .id = RBH_BI_MPI_FILE,
    .name = RBH_MPI_FILE_BACKEND_NAME,
    .ops = &MPI_FILE_BACKEND_OPS,
};

static void
mpi_file_backend_init(struct mpi_file_backend *mpi_file)
{
    int flag;

    MPI_Initialized(&flag);
    if (!flag) {
        MPI_Init(NULL, NULL);
        mfu_init();
    }

    mpi_file->flist = mfu_flist_new();
    if (mpi_file->flist == NULL)
        error(EXIT_FAILURE, errno, "malloc flist");
}

struct rbh_backend *
rbh_mpi_file_backend_new(const char *path)
{
    struct mpi_file_backend *mpi_file;

    mpi_file = malloc(sizeof(*mpi_file));
    if (mpi_file == NULL)
        error(EXIT_FAILURE, errno, "malloc mpi_file_backend struct");

    mpi_file->name = strdup(*path == '\0' ? "mpi-file.mfu" : path);
    if (mpi_file->name == NULL) {
        int save_errno = errno;
        free(mpi_file);
        errno = save_errno;
        return NULL;
    }

    mpi_file_backend_init(mpi_file);
    mpi_file->backend = MPI_FILE_BACKEND;

    return &mpi_file->backend;
}
