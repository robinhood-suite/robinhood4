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

#include <robinhood/backends/iter_mpi_internal.h>
#include <robinhood/backends/posix_internal.h>
#include <robinhood/mpi_rc.h>

struct mfu_iterator {
    struct posix_iterator posix;
    /** Index of current file in the flist in the process */
    uint64_t current;
    /**
     * Size of the flist in the process, this is not the global size of the
     * flist
     */
    uint64_t total;
    /** List of files processed by a given MPI process  */
    mfu_flist files;
    bool is_branch;
};

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
        return NULL;
    }

    path = mfu_flist_file_get_name(iter->files, iter->current);
    path_dup = strdup(path);
    if (path_dup == NULL)
        return NULL;

    fi.path = path;
    fi.name = basename(path_dup);
    /* FIXME for now we don't support mfu files so we hardcode the use of fd */
    fi.parent_id = get_parent_id(path, true, iter->posix.prefix_len);

    if (fi.parent_id == NULL) {
        fprintf(stderr, "Failed to get parent id of '%s'\n", path);
        free(path_dup);
        if (skip_error) {
            fprintf(stderr, "Synchronization of '%s' skipped\n", path);
            iter->current++;
            goto skip;
        }
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

    fsentry = fsentry_from_fi(&fi, &iter->posix);

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
        return NULL;
    }

    iter->current++;

    free(path_dup);
    free(fi.parent_id);

    return fsentry;
}

static void
mpi_finalize(void)
{
    int initialized;
    int finalized;

    /* Prevents finalizing twice MPI if we use two backends with MPI */
    MPI_Initialized(&initialized);
    MPI_Finalized(&finalized);

    if (initialized && !finalized) {
        mfu_finalize();
        MPI_Finalize();
    }
}

static void
mfu_iter_destroy(void *iterator)
{
    struct mfu_iterator *iter = iterator;

    mfu_flist_free(&iter->files);
    free(iter);
    rbh_mpi_dec_ref(mpi_finalize);
}

static const struct rbh_mut_iterator_operations MFU_ITER_OPS = {
    .next    = mfu_iter_next,
    .destroy = mfu_iter_destroy,
};

static const struct rbh_mut_iterator MFU_ITER = {
    .ops = &MFU_ITER_OPS,
};

static void
mpi_initialize(void)
{
    int initialized;

    MPI_Initialized(&initialized);
    if (!initialized) {
        MPI_Init(NULL, NULL);
        mfu_init();
    }
}

struct rbh_mut_iterator *
rbh_posix_mfu_iter_new(const char *root,
                       const char *entry,
                       int statx_sync_type)
{
    struct mfu_iterator *mfu;
    int save_errno;
    int rc;

    rbh_mpi_inc_ref(mpi_initialize);

    mfu = malloc(sizeof(*mfu));
    if (!mfu)
        return NULL;

    rc = posix_iterator_setup(&mfu->posix, root, entry, statx_sync_type);
    save_errno = errno;
    if (rc == -1)
        goto free_iter;

    mfu->posix.iterator = MFU_ITER;
    mfu->files = walk_path(mfu->posix.path);
    mfu->total = mfu_flist_size(mfu->files);
    mfu->current = 0;
    mfu->is_branch = (entry != NULL);

    free(mfu->posix.path);

    return (struct rbh_mut_iterator *)mfu;

free_iter:
    free(mfu);
    errno = save_errno;

    return NULL;
}
