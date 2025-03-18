/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <mfu.h>
#include <sys/xattr.h>

#include "mfu_internals.h"

#include <robinhood/backends/mfu.h>
#include <robinhood/backends/posix_extension.h>
#include <robinhood/mpi_rc.h>

static void *
mfu_iter_next(void *_iter)
{
    struct mfu_iterator *iter = _iter;
    bool skip_error = iter->posix.skip_error;
    struct rbh_fsentry *fsentry = NULL;
    struct file_info fi;
    const char *path;
    char *path_dup;
    int rank;

skip:
    if (iter->current == iter->total) {
        errno = ENODATA;
        /* the flist belongs to the backend, do not free it */
        iter->files = NULL;
        return NULL;
    }

    path = mfu_flist_file_get_name(iter->files, iter->current);
    path_dup = strdup(path);
    if (path_dup == NULL) {
        /* the flist belongs to the backend, do not free it */
        iter->files = NULL;
        return NULL;
    }

    fi.path = path;
    fi.name = basename(path_dup);
    fi.parent_id = get_parent_id(path, !iter->is_mpifile,
                                 iter->posix.prefix_len,
                                 RBH_BI_POSIX);

    if (fi.parent_id == NULL) {
        fprintf(stderr, "Failed to get parent id of '%s'\n", path);
        free(path_dup);
        if (skip_error) {
            fprintf(stderr, "Synchronization of '%s' skipped\n", path);
            iter->current++;
            goto skip;
        }

        /* the flist belongs to the backend, do not free it */
        iter->files = NULL;
        return NULL;
    }

    /* Modify the root's name and parent ID to match RobinHood's conventions,
     * only if we are not synchronizing a branch
     */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0 && iter->current == 0 && !iter->is_branch) {
        free(fi.parent_id);
        fi.parent_id = rbh_id_new(NULL, 0);
        fi.name[0] = '\0';
    }

    fsentry = iter->fsentry_new(&fi, &iter->posix);

    if (fsentry == NULL && (errno == ENOENT || errno == ESTALE)) {
        /* The entry moved from under our feet */
        free(path_dup);
        free(fi.parent_id);
        if (skip_error) {
            fprintf(stderr, "Synchronization of '%s' skipped\n",
                    path);
            iter->current++;
            goto skip;
        }
        /* the flist belongs to the backend, do not free it */
        iter->files = NULL;
        return NULL;
    }

    iter->current++;

    free(path_dup);
    free(fi.parent_id);

    return fsentry;
}

static void
mfu_iter_destroy(void *iterator)
{
    struct mfu_iterator *iter = iterator;

    if (iter->files)
        mfu_flist_free(&iter->files);

    free(iter);
    rbh_mpi_dec_ref(rbh_mpi_finalize);
}

static const struct rbh_mut_iterator_operations MFU_ITER_OPS = {
    .next    = mfu_iter_next,
    .destroy = mfu_iter_destroy,
};

static const struct rbh_mut_iterator MFU_ITER = {
    .ops = &MFU_ITER_OPS,
};

static struct rbh_mut_iterator *
mfu_iter_new(const char *root, const char *entry, int statx_sync_type,
             size_t prefix_len, mfu_flist flist)
{
    struct mfu_iterator *mfu;
    int save_errno;
    int rc;

    rbh_mpi_inc_ref(rbh_mpi_initialize);

    mfu = malloc(sizeof(*mfu));
    if (!mfu)
        return NULL;

    if (flist) {
        mfu->posix.prefix_len = prefix_len;
        mfu->files = flist;
        mfu->is_mpifile = true;
        /* Will be setup by caller in mpi-file backend */
        mfu->fsentry_new = NULL;
    } else {
        rc = posix_iterator_setup(&mfu->posix, root, entry, statx_sync_type);
        save_errno = errno;
        if (rc == -1)
            goto free_iter;

        mfu->files = walk_path(mfu->posix.path);
        mfu->is_mpifile = false;
        mfu->fsentry_new = fsentry_from_fi;

        free(mfu->posix.path);
    }

    mfu->posix.iterator = MFU_ITER;
    mfu->total = mfu_flist_size(mfu->files);
    mfu->current = 0;
    mfu->is_branch = (entry != NULL);

    return (struct rbh_mut_iterator *)mfu;

free_iter:
    free(mfu);
    errno = save_errno;

    return NULL;
}

struct rbh_mut_iterator *
rbh_posix_mfu_iter_new(const char *root,
                       const char *entry,
                       int statx_sync_type)
{
    return mfu_iter_new(root, entry, statx_sync_type, 0, NULL);
}

struct rbh_mut_iterator *
rbh_mpi_file_mfu_iter_new(mfu_flist flist, size_t prefix_len)
{
    return mfu_iter_new(NULL, NULL, 0, prefix_len, flist);
}
