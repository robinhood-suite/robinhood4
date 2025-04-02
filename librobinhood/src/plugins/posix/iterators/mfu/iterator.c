/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <mfu.h>
#include <sys/xattr.h>
#include <libgen.h>

#include "mfu_internals.h"

#include <robinhood/backends/mfu.h>
#include <robinhood/backends/posix_extension.h>
#include <robinhood/mpi_rc.h>

static __thread struct rbh_id *current_parent_id = NULL;
static __thread char *current_parent = NULL;
static __thread struct rbh_sstack *sstack;
static __thread int current_children = 0;

static struct rbh_id ROOT_PARENT_ID = {
    .data = NULL,
    .size = 0,
};

__attribute__((destructor))
static void
free_sstack(void)
{
    if (sstack)
        rbh_sstack_destroy(sstack);
}

static void *
mfu_iter_next(void *_iter)
{
    struct mfu_iterator *iter = _iter;
    bool skip_error = iter->posix.skip_error;
    struct rbh_fsentry *fsentry = NULL;
    struct file_info fi;
    const char *path;
    char *parent_dup;
    char *path_dup;
    char *parent;
    int rank;

    if (sstack == NULL) {
        sstack = rbh_sstack_new(1 << 10);
        if (sstack == NULL)
            return NULL;
    }

skip:
    if (iter->current == iter->total) {
        /* Generate a fsevent for the last directory we have explored to update
         * its children counter.
         */
        if (current_parent_id != NULL) {
            fsentry = build_fsentry_nb_children(current_parent_id,
                                                current_children, sstack);

            free(current_parent_id);

            current_parent = NULL;
            current_parent_id = NULL;

            return fsentry;
        }

        errno = ENODATA;
        /* the flist belongs to the backend, do not free it */
        iter->files = NULL;
        return NULL;
    }

    path = mfu_flist_file_get_name(iter->files, iter->current);

    parent_dup = RBH_SSTACK_PUSH(sstack, path, strlen(path) + 1);
    parent = dirname(parent_dup);

    /* Setup the current_parent path/ID */
    if (current_parent == NULL) {
        current_parent = RBH_SSTACK_PUSH(sstack, parent, strlen(parent) + 1);
        current_parent_id = get_parent_id(path, !iter->is_mpifile,
                                          iter->posix.prefix_len,
                                          iter->backend_id);
    }

    /* If parent is different of current_parent, it means that we are iterating
     * entries of a new directory. We need to get the new parent path/ID and
     * generate a fsevent to update the children counter of the directory we are
     * existing.
     */
    if (strcmp(current_parent, parent) != 0) {
        struct rbh_id *tmp_id = current_parent_id;
        int tmp_children = current_children;

        current_children = 0;
        current_parent = RBH_SSTACK_PUSH(sstack, parent, strlen(parent) + 1);
        current_parent_id = get_parent_id(path, !iter->is_mpifile,
                                          iter->posix.prefix_len,
                                          iter->backend_id);

        if (!rbh_id_equal(tmp_id, &ROOT_PARENT_ID))
            fsentry = build_fsentry_nb_children(tmp_id, tmp_children, sstack);

        free(tmp_id);

        if (fsentry)
            return fsentry;
        else
            current_children++;
    } else {
        current_children++;
    }

    fi.path = path;
    path_dup = RBH_SSTACK_PUSH(sstack, path, strlen(path) + 1);

    /* Modify the root's name and parent ID to match RobinHood's conventions,
     * only if we are not synchronizing a branch
     */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0 && iter->current == 0 && !iter->is_branch) {
        free(current_parent_id);
        current_parent_id = rbh_id_new(NULL, 0);
        fi.parent_id = current_parent_id;
        fi.name = "\0";
    } else {
        fi.name = basename(path_dup);
        fi.parent_id = current_parent_id;
    }

    if (fi.parent_id == NULL) {
        fprintf(stderr, "Failed to get parent id of '%s'\n", path);
        if (skip_error) {
            fprintf(stderr, "Synchronization of '%s' skipped\n", path);
            iter->current++;
            current_children--;
            goto skip;
        }

        /* the flist belongs to the backend, do not free it */
        iter->files = NULL;
        return NULL;
    }

    fsentry = iter->fsentry_new(&fi, &iter->posix);

    if (fsentry == NULL && (errno == ENOENT || errno == ESTALE)) {
        /* The entry moved from under our feet */
        if (skip_error) {
            fprintf(stderr, "Synchronization of '%s' skipped\n",
                    path);
            iter->current++;
            current_children--;
            goto skip;
        }
        /* the flist belongs to the backend, do not free it */
        iter->files = NULL;
        return NULL;
    }

    iter->current++;

    return fsentry;
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
        mfu_finalize();
        MPI_Finalize();
    }
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

void
rbh_mpi_initialize(void)
{
    int initialized;

    MPI_Initialized(&initialized);
    if (!initialized) {
        MPI_Init(NULL, NULL);
        mfu_init();
    }
}

static struct rbh_mut_iterator *
mfu_iter_new(const char *root, const char *entry, int statx_sync_type,
             size_t prefix_len, mfu_flist flist, short backend_id)
{
    struct mfu_iterator *mfu;
    int save_errno;
    char *path;
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
        path = realpath(root, NULL);
        if (path == NULL)
            goto free_iter;

        rc = posix_iterator_setup(&mfu->posix, path, entry, statx_sync_type);
        save_errno = errno;
        if (rc == -1)
            goto free_iter;

        mfu->files = walk_path(mfu->posix.path);
        mfu->is_mpifile = false;
        mfu->fsentry_new = fsentry_from_fi;

        free(path);
        free(mfu->posix.path);
    }

    mfu->posix.iterator = MFU_ITER;
    mfu->backend_id = backend_id;
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
    return mfu_iter_new(root, entry, statx_sync_type, 0, NULL, RBH_BI_POSIX);
}

struct rbh_mut_iterator *
rbh_mpi_file_mfu_iter_new(mfu_flist flist, size_t prefix_len)
{
    return mfu_iter_new(NULL, NULL, 0, prefix_len, flist, RBH_BI_MPI_FILE);
}
