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

#include "robinhood/backends/posix.h"
#include "robinhood/backends/posix_internal.h"
#include "robinhood/backends/lustre_mpi.h"
#include "robinhood/backends/lustre_internal.h"
#include "robinhood/backends/iter_mpi_internal.h"

static struct rbh_backend *
lustre_mpi_backend_branch(void *backend, const struct rbh_id *id,
                          const char *path);

/*----------------------------------------------------------------------------*
 |                          lustre_mpi_iterator                               |
 *----------------------------------------------------------------------------*/

static struct rbh_mut_iterator *
lustre_mpi_iterator_new(const char *root, const char *entry,
                        int statx_sync_type)
{
    struct mpi_iterator *mpi_iter;

    mpi_iter = (struct mpi_iterator *)
                mpi_iterator_new(root, entry, statx_sync_type);
    if (mpi_iter == NULL)
        return NULL;

    mpi_iter->inode_xattrs_callback = lustre_inode_xattrs_callback;

    return (struct rbh_mut_iterator *)mpi_iter;
}

/*----------------------------------------------------------------------------*
 |                          lustre_mpi_backend                                |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                          filter()                                  |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
lustre_mpi_backend_filter(
    void *backend, const struct rbh_filter *filter,
    const struct rbh_filter_options *options,
    __attribute__((unused)) const struct rbh_filter_output *output)
{
    return mpi_backend_filter(backend, filter, options);
}

    /* -------------------------------------------------------------------*
     |                          get_attribute()                           |
     *--------------------------------------------------------------------*/

static int
lustre_mpi_backend_get_attribute(void *backend, uint64_t flags,
                                 void *arg, struct rbh_value_pair *pairs,
                                 int available_pairs)
{
    (void) backend;
    return lustre_get_attribute(flags, arg, pairs, available_pairs);
}


    /* -------------------------------------------------------------------*
     |                          get_option()                              |
     *--------------------------------------------------------------------*/

static int
lustre_mpi_backend_get_option(void *backend, unsigned int option, void *data,
                              size_t *data_size)
{
    return posix_backend_get_option(backend, option, data, data_size);
}

    /* -------------------------------------------------------------------*
    |                          set_option()                               |
    *---------------------------------------------------------------------*/

static int
lustre_mpi_backend_set_option(void *backend, unsigned int option,
                              const void *data, size_t data_size)
{
    return posix_backend_set_option(backend, option, data, data_size);
}

    /* -------------------------------------------------------------------*
     |                          destroy()                                 |
     *--------------------------------------------------------------------*/

static void
lustre_mpi_backend_destroy(void *backend)
{
    posix_backend_destroy(backend);
}

    /* -------------------------------------------------------------------*
    |                          root()                                     |
    *---------------------------------------------------------------------*/

static struct rbh_fsentry *
lustre_mpi_backend_root(void *backend,
                        const struct rbh_filter_projection *projection)
{
    static const struct rbh_backend_operations LUSTRE_MPI_ROOT_BACKEND_OPS = {
        .get_option = lustre_mpi_backend_get_option,
        .set_option = lustre_mpi_backend_set_option,
        .branch = lustre_mpi_backend_branch,
        .root = lustre_mpi_backend_root,
        .filter = posix_backend_filter,
        .get_attribute = lustre_mpi_backend_get_attribute,
        .destroy = lustre_mpi_backend_destroy,
    };
    struct posix_backend *lustre_mpi_root = backend;

    (void) backend;

    lustre_mpi_root->iter_new = lustre_iterator_new;
    lustre_mpi_root->backend.ops = &LUSTRE_MPI_ROOT_BACKEND_OPS;

    return posix_root(backend, projection);
}

    /*--------------------------------------------------------------------*
     |                          branch()                                  |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
lustre_mpi_branch_backend_filter(
    void *backend, const struct rbh_filter *filter,
    const struct rbh_filter_options *options,
    __attribute__((unused)) const struct rbh_filter_output *output)
{
    return mpi_branch_backend_filter(backend, filter, options);
}

static const struct rbh_backend_operations LUSTRE_MPI_BRANCH_BACKEND_OPS = {
    .root = lustre_mpi_backend_root,
    .branch = lustre_mpi_backend_branch,
    .filter = lustre_mpi_branch_backend_filter,
    .destroy = lustre_mpi_backend_destroy,
};

static const struct rbh_backend LUSTRE_MPI_BRANCH_BACKEND = {
    .name = RBH_LUSTRE_MPI_BACKEND_NAME,
    .ops = &LUSTRE_MPI_BRANCH_BACKEND_OPS,
};

static struct rbh_backend *
lustre_mpi_backend_branch(void *backend, const struct rbh_id *id,
                          const char *path)
{
    struct posix_backend *lustre_mpi = backend;
    struct posix_branch_backend *branch;

    branch = (struct posix_branch_backend *)
              posix_backend_branch(backend, id, path);
    if (branch == NULL)
        return NULL;

    branch->posix.statx_sync_type = lustre_mpi->statx_sync_type;
    branch->posix.backend = LUSTRE_MPI_BRANCH_BACKEND;
    branch->posix.iter_new = lustre_mpi_iterator_new;

    return &branch->posix.backend;
}

    /*--------------------------------------------------------------------*
     |                          backend()                                 |
     *--------------------------------------------------------------------*/

static const struct rbh_backend_operations LUSTRE_MPI_BACKEND_OPS = {
    .get_option = lustre_mpi_backend_get_option,
    .set_option = lustre_mpi_backend_set_option,
    .branch = lustre_mpi_backend_branch,
    .root = lustre_mpi_backend_root,
    .filter = lustre_mpi_backend_filter,
    .get_attribute = lustre_mpi_backend_get_attribute,
    .destroy = lustre_mpi_backend_destroy,
};

struct rbh_backend *
rbh_lustre_mpi_backend_new(const struct rbh_backend_plugin *self,
                           const char *type,
                           const char *path,
                           struct rbh_config *config)
{
    struct posix_backend *lustre_mpi;
    int flag;

    MPI_Initialized(&flag);
    if (!flag) {
        MPI_Init(NULL, NULL);
        mfu_init();
    }

    lustre_mpi = (struct posix_backend *)rbh_posix_backend_new(self, type, path,
                                                               config);
    if (lustre_mpi == NULL)
        return NULL;

    lustre_mpi->iter_new = lustre_mpi_iterator_new;
    lustre_mpi->backend.name = RBH_LUSTRE_MPI_BACKEND_NAME;
    lustre_mpi->backend.ops = &LUSTRE_MPI_BACKEND_OPS;
    lustre_mpi->backend.id = RBH_BI_LUSTRE_MPI;

    return &lustre_mpi->backend;
}
