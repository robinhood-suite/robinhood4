/* This file is part of RobinHood 4
 *
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/backend.h"
#include "robinhood/backends/posix.h"
#include "robinhood/backends/posix_internal.h"
#include "robinhood/backends/posix_mpi.h"
#include "robinhood/backends/posix_mpi_internal.h"
#include "robinhood/backends/iter_mpi_internal.h"

/*----------------------------------------------------------------------------*
 |                            mpi_iterator                                    |
 *----------------------------------------------------------------------------*/

static struct rbh_mut_iterator *
posix_mpi_iterator_new(const char *root, const char *entry,
                       int statx_sync_type)
{
    struct mpi_iterator *mpi_iter;

    mpi_iter = (struct mpi_iterator *)
                mpi_iterator_new(root, entry, statx_sync_type);
    if (mpi_iter == NULL)
        return NULL;

    mpi_iter->backend_id = RBH_BI_POSIX;
    mpi_iter->inode_xattrs_callback = NULL;

    return (struct rbh_mut_iterator *)mpi_iter;
}

/*----------------------------------------------------------------------------*
 |                          posix_mpi_backend                                 |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                          filter()                                  |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
posix_mpi_backend_filter(
    void *backend, const struct rbh_filter *filter,
    const struct rbh_filter_options *options,
    __attribute__((unused)) const struct rbh_filter_output *output)
{
    return mpi_backend_filter(backend, filter, options);
}

    /*--------------------------------------------------------------------*
     |                         get_option()                               |
     *--------------------------------------------------------------------*/

static int
posix_mpi_backend_get_option(void *backend, unsigned int option, void *data,
                             size_t *data_size)
{
    return posix_backend_get_option(backend, option, data, data_size);
}

    /*--------------------------------------------------------------------*
     |                          set_option()                              |
     *--------------------------------------------------------------------*/

static int
posix_mpi_backend_set_option(void *backend, unsigned int option,
                             const void *data, size_t data_size)
{
    return posix_backend_set_option(backend, option, data, data_size);
}

    /*--------------------------------------------------------------------*
     |                          destroy()                                 |
     *--------------------------------------------------------------------*/

static void
posix_mpi_backend_destroy(void *backend)
{
    posix_backend_destroy(backend);
}

void
rbh_posix_mpi_plugin_destroy()
{
    mfu_finalize();
    MPI_Finalize();
}

    /*--------------------------------------------------------------------*
     |                           root()                                   |
     *--------------------------------------------------------------------*/

static struct rbh_fsentry *
posix_mpi_backend_root(void *backend,
                       const struct rbh_filter_projection *projection)
{
    static const struct rbh_backend_operations POSIX_MPI_ROOT_BACKEND_OPS = {
        .get_option = posix_mpi_backend_get_option,
        .set_option = posix_mpi_backend_set_option,
        .branch = posix_mpi_backend_branch,
        .root = posix_mpi_backend_root,
        .filter = posix_backend_filter,
        .destroy = posix_mpi_backend_destroy,
    };
    struct posix_backend *posix_mpi_root = backend;

    posix_mpi_root->iter_new = posix_iterator_new;
    posix_mpi_root->backend.ops = &POSIX_MPI_ROOT_BACKEND_OPS;

    return posix_root(backend, projection);
}

    /*--------------------------------------------------------------------*
     |                          branch()                                  |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
posix_mpi_branch_backend_filter(
    void *backend, const struct rbh_filter *filter,
    const struct rbh_filter_options *options,
    __attribute__((unused)) const struct rbh_filter_output *output)
{
    return mpi_branch_backend_filter(backend, filter, options);
}

static const struct rbh_backend_operations POSIX_MPI_BRANCH_BACKEND_OPS = {
    .root = posix_mpi_backend_root,
    .branch = posix_mpi_backend_branch,
    .filter = posix_mpi_branch_backend_filter,
    .destroy = posix_mpi_backend_destroy,
};

static const struct rbh_backend POSIX_MPI_BRANCH_BACKEND = {
    .name = RBH_POSIX_MPI_BACKEND_NAME,
    .ops = &POSIX_MPI_BRANCH_BACKEND_OPS,
};

struct rbh_backend *
posix_mpi_backend_branch(void *backend, const struct rbh_id *id,
                         const char *path)
{
    struct posix_backend *posix_mpi = backend;
    struct posix_branch_backend *branch;

    branch = (struct posix_branch_backend *)
              posix_backend_branch(backend, id, path);
    if (branch == NULL)
        return NULL;

    branch->posix.statx_sync_type = posix_mpi->statx_sync_type;
    branch->posix.backend = POSIX_MPI_BRANCH_BACKEND;
    branch->posix.iter_new = posix_mpi_iterator_new;

    return &branch->posix.backend;
}

    /*--------------------------------------------------------------------*
     |                          backend()                                 |
     *--------------------------------------------------------------------*/

static const struct rbh_backend_operations POSIX_MPI_BACKEND_OPS = {
    .get_option = posix_mpi_backend_get_option,
    .set_option = posix_mpi_backend_set_option,
    .branch = posix_mpi_backend_branch,
    .root = posix_mpi_backend_root,
    .filter = posix_mpi_backend_filter,
    .destroy = posix_mpi_backend_destroy,
};

static const struct rbh_value POSIX_MPI_STRING_TYPE = {
    .type = RBH_VT_STRING,
    .string = "mpi"
};

static const struct rbh_value POSIX_STRING_TYPE = {
    .type = RBH_VT_STRING,
    .string = "posix",
};

static const struct rbh_value values[] = {
    POSIX_MPI_STRING_TYPE,
    POSIX_STRING_TYPE,
};

static const struct rbh_value RBH_POSIX_MPI_BACKEND_TYPE = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .values = values,
        .count = 2,
    },
};

struct rbh_backend *
rbh_posix_mpi_backend_new(const char *path, struct rbh_config *config)
{
    struct posix_backend *posix_mpi;
    int flag;

    MPI_Initialized(&flag);
    if (!flag) {
        MPI_Init(NULL, NULL);
        mfu_init();
    }

    posix_mpi = (struct posix_backend *)rbh_posix_backend_new(path, config);
    if (posix_mpi == NULL)
        return NULL;

    posix_mpi->iter_new = posix_mpi_iterator_new;
    posix_mpi->backend.name = RBH_POSIX_MPI_BACKEND_NAME;
    posix_mpi->backend.backend_type = &RBH_POSIX_MPI_BACKEND_TYPE;
    posix_mpi->backend.ops = &POSIX_MPI_BACKEND_OPS;
    posix_mpi->backend.id = RBH_BI_POSIX_MPI;

    return &posix_mpi->backend;
}
