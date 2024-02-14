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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "robinhood/backends/lustre_mpi_internal.h"
#include "robinhood/fsentry.h"
#include "robinhood/iterator.h"

/*----------------------------------------------------------------------------*
 |                                mfu_iterator                                |
 *----------------------------------------------------------------------------*/

static void
create_flist_by_path(const char* path, mfu_flist flist)
{
    mfu_walk_opts_t *walk_opts = mfu_walk_opts_new();
    mfu_file_t *mfu_file = mfu_file_new();

    /* Tell mpifileutils not to do stats during the walk  */
    walk_opts->use_stat = 0;

    mfu_flist_walk_path(path, walk_opts, flist, mfu_file);

    mfu_walk_opts_delete(&walk_opts);
    mfu_file_delete(&mfu_file);
}

static void *
lustre_mpi_iter_next(void *iterator)
{
    struct mpi_iterator *mpi_iter = iterator;
    bool skip_error = mpi_iter->skip_error;
    struct rbh_fsentry *fsentry = NULL;
    const char *path;
skip:
    if (mpi_iter->current == mpi_iter->total) {
        errno = ENODATA;
        return NULL;
    }

    path = mfu_flist_file_get_name(mpi_iter->flist, mpi_iter->current);
    mpi_iter->current++;

    if (path == NULL) {
        if (skip_error)
            goto skip;
        return NULL;
    }

    /* TO-DO construct fsentry from flist path *
     *
     * fsentry = fsentry_from_flist_path()    */

    if (fsentry == NULL && (errno == ENOENT || errno == ESTALE)) {
        /* The entry moved from under our feet */
        if (skip_error) {
            fprintf(stderr, "Synchronization of '%s' skipped\n",
                    path);
            goto skip;
        }
        return NULL;
    }
    return fsentry;
}

static void
lustre_mpi_iter_destroy(void *iterator)
{
    struct mpi_iterator *mpi_iter = iterator;
    mfu_flist_free(mpi_iter->flist);
    free(mpi_iter);
}

static const struct rbh_mut_iterator_operations LUSTRE_MPI_ITER_OPS = {
    .next = lustre_mpi_iter_next,
    .destroy = lustre_mpi_iter_destroy,
};

static const struct rbh_mut_iterator LUSTRE_MPI_ITER = {
    .ops = &LUSTRE_MPI_ITER_OPS,
};

struct mpi_iterator *
lustre_mpi_iterator_new(const char *root, const char *entry,
                        int statx_sync_type)
{
    mfu_flist flist = mfu_flist_new();
    struct mpi_iterator *mpi_iter;
    char *paths[1] = {NULL};

    /* `root' must not be empty, nor end with a '/' (except if `root' == "/")
     *
     * Otherwise, the "path" xattr will not be correct
     */

    assert(strlen(root) > 0);
    assert(strcmp(root, "/") == 0 || root[strlen(root) - 1] != '/');

    if (entry == NULL) {
        paths[0] = strdup(root);
    } else {
        assert(strcmp(root, "/") == 0 || *entry == '/' || *entry == '\0');
        if (asprintf(&paths[0], "%s%s", root, entry) < 0)
            paths[0] = NULL;
    }

    if (paths[0] == NULL)
            return NULL;

    mpi_iter = malloc(sizeof(*mpi_iter));
    if (mpi_iter == NULL) {
        int save_errno = errno;
        free(paths[0]);
        errno = save_errno;
        return NULL;
    }

    create_flist_by_path(paths[0], flist);

    mpi_iter->prefix_len = strcmp(root, "/") ? strlen(root) : 0;
    mpi_iter->flist = flist;
    mpi_iter->total = mfu_flist_size(flist);
    mpi_iter->statx_sync_type = statx_sync_type;
    mpi_iter->iterator = LUSTRE_MPI_ITER;
    mpi_iter->ns_xattrs_callback = NULL;
    mpi_iter->current = 0;

    free(paths[0]);

    return mpi_iter;
}
