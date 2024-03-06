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
#include <error.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <libgen.h>

#include "robinhood/backends/lustre_mpi_internal.h"
#include "robinhood/backends/posix_internal.h"
#include "robinhood/backends/posix.h"
#include "robinhood/backends/lustre_mpi.h"
#include "robinhood/backends/lustre_internal.h"

/*----------------------------------------------------------------------------*
 |                                mpi_iterator                                |
 *----------------------------------------------------------------------------*/

static struct rbh_id ROOT_PARENT_ID = {
    .data = NULL,
    .size = 0,
};

static struct rbh_id *
get_parent_id(const char *path)
{
    struct rbh_id *parent_id;
    int save_errno = errno;
    char *parent_path;
    char *tmp_path;
    int fd;

    tmp_path = strdup(path);
    if (tmp_path == NULL)
        return NULL;
    parent_path = dirname(tmp_path);

    fd = openat(AT_FDCWD, parent_path, O_RDONLY | O_CLOEXEC | O_PATH);
    if (fd < 0) {
        save_errno = errno;
        free(tmp_path);
        errno = save_errno;
        return NULL;
    }

    parent_id = id_from_fd(fd);
    if (parent_id == NULL)
        return NULL;

    save_errno = errno;
    close(fd);
    free(tmp_path);
    errno = save_errno;

    return parent_id;
}

static struct rbh_fsentry *
fsentry_from_mpi_fi(struct mpi_file_info *mpi_fi, int statx_sync_type,
                    size_t prefix_len,
                    int (*inode_xattrs_callback)(const int,
                                                 const struct rbh_statx *,
                                                 struct rbh_value_pair *,
                                                 ssize_t *,
                                                 struct rbh_value_pair *,
                                                 struct rbh_sstack *))
{
    const struct rbh_value path = {
        .type = RBH_VT_STRING,
        .string = strlen(mpi_fi->path) == prefix_len ?
            "/" : mpi_fi->path + prefix_len,
    };
    struct fsentry_id_pair pair;
    bool fsentry_success;

    fsentry_success = fsentry_from_any(&pair, &path, (char *)mpi_fi->path,
                                       NULL, mpi_fi->parent_id, mpi_fi->name,
                                       statx_sync_type, inode_xattrs_callback);
    if (!fsentry_success)
        return NULL;

    return pair.fsentry;
}

static mfu_flist
walk_path(const char* path)
{
    mfu_walk_opts_t *walk_opts = mfu_walk_opts_new();
    mfu_file_t *mfu_file = mfu_file_new();
    mfu_flist flist = mfu_flist_new();

    if (walk_opts == NULL || mfu_file == NULL || flist == NULL)
        error(EXIT_FAILURE, errno, "malloc flist, mfu_file or walk_opts");

    /* Tell mpifileutils not to do stats during the walk */
    walk_opts->use_stat = 0;

    mfu_flist_walk_path(path, walk_opts, flist, mfu_file);

    mfu_walk_opts_delete(&walk_opts);
    mfu_file_delete(&mfu_file);

    return flist;
}

static void *
lustre_mpi_iter_next(void *iterator)
{
    struct mpi_iterator *mpi_iter = iterator;
    bool skip_error = mpi_iter->skip_error;
    struct rbh_fsentry *fsentry = NULL;
    struct mpi_file_info mpi_fi;
    const char *path;
    char *path_dup;
    int rank;

skip:
    if (mpi_iter->current == mpi_iter->total) {
        errno = ENODATA;
        return NULL;
    }

    path = mfu_flist_file_get_name(mpi_iter->flist, mpi_iter->current);
    path_dup = strdup(path);
    if (path_dup == NULL)
        return NULL;

    mpi_fi.path = path;
    mpi_fi.name = basename(path_dup);
    mpi_fi.parent_id = get_parent_id(path);

    if (mpi_fi.parent_id == NULL){
        fprintf(stderr, "Failed to get parent id of '%s'\n", path);
        free(path_dup);
        if (skip_error) {
            fprintf(stderr, "Synchronization of '%s' skipped\n", path);
            mpi_iter->current++;
            goto skip;
        }
        return NULL;
    }

    /* Modify the root's name and parent ID to match RobinHood's conventions,
     * only if we are not synchronizing a branch*/
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0 && mpi_iter->current == 0 && !mpi_iter->is_branch) {
        mpi_fi.parent_id = &ROOT_PARENT_ID;
        mpi_fi.name[0] = '\0';
    }

    fsentry = fsentry_from_mpi_fi(&mpi_fi, mpi_iter->statx_sync_type,
                                  mpi_iter->prefix_len,
                                  mpi_iter->inode_xattrs_callback);
    if (fsentry == NULL && (errno == ENOENT || errno == ESTALE)) {
        /* The entry moved from under our feet */
        free(path_dup);
        if (skip_error) {
            fprintf(stderr, "Synchronization of '%s' skipped\n",
                    path);
            mpi_iter->current++;
            goto skip;
        }
        return NULL;
    }

    mpi_iter->current++;
    free(path_dup);

    return fsentry;
}

static void
lustre_mpi_iter_destroy(void *iterator)
{
    struct mpi_iterator *mpi_iter = iterator;
    mfu_flist_free(&mpi_iter->flist);
    free(mpi_iter);
}

static const struct rbh_mut_iterator_operations LUSTRE_MPI_ITER_OPS = {
    .next = lustre_mpi_iter_next,
    .destroy = lustre_mpi_iter_destroy,
};

static const struct rbh_mut_iterator LUSTRE_MPI_ITER = {
    .ops = &LUSTRE_MPI_ITER_OPS,
};

static void *
lustre_mpi_iterator_new(const char *root, const char *entry,
                        int statx_sync_type)
{
    struct mpi_iterator *mpi_iter;
    char *path = NULL;

    /* `root' must not be empty, nor end with a '/' (except if `root' == "/")
     *
     * Otherwise, the "path" xattr will not be correct
     */

    assert(strlen(root) > 0);
    assert(strcmp(root, "/") == 0 || root[strlen(root) - 1] != '/');

    if (entry == NULL) {
        path = strdup(root);
    } else {
        assert(strcmp(root, "/") == 0 || *entry == '/' || *entry == '\0');
        if (asprintf(&path, "%s%s", root, entry) < 0)
            path = NULL;
    }

    if (path == NULL)
        return NULL;

    mpi_iter = malloc(sizeof(*mpi_iter));
    if (mpi_iter == NULL)
        error(EXIT_FAILURE, errno, "malloc mpi_iterator");

    mpi_iter->inode_xattrs_callback = lustre_inode_xattrs_callback;
    mpi_iter->prefix_len = strcmp(root, "/") ? strlen(root) : 0;
    mpi_iter->statx_sync_type = statx_sync_type;
    mpi_iter->iterator = LUSTRE_MPI_ITER;
    mpi_iter->flist = walk_path(path);
    mpi_iter->total = mfu_flist_size(mpi_iter->flist);
    mpi_iter->current = 0;

    free(path);
    return mpi_iter;
}

/*----------------------------------------------------------------------------*
 |                          lustre_mpi_backend                                |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                          filter()                                  |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
lustre_mpi_backend_filter(void *backend, const struct rbh_filter *filter,
                          const struct rbh_filter_options *options)
{
    struct posix_backend *lustre_mpi = backend;
    struct mpi_iterator *mpi_iter;
    uint64_t global_size;
    int save_errno;

    if (filter != NULL) {
        errno = ENOTSUP;
        return NULL;
    }

    if (options->skip > 0 || options->limit > 0 || options->sort.count > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    mpi_iter = (struct mpi_iterator *)
                lustre_mpi->iter_new(lustre_mpi->root, NULL,
                                     lustre_mpi->statx_sync_type);
    if (mpi_iter == NULL)
        return NULL;
    mpi_iter->skip_error = options->skip_error;

    global_size = mfu_flist_global_size(mpi_iter->flist);
    /* Only if the walk failed, the root doesn't exist */
    if (global_size == 0) {
        errno = ENOENT;
        goto out_destroy_iter;
    }
    return &mpi_iter->iterator;

out_destroy_iter:
    save_errno = errno;
    rbh_mut_iter_destroy(&mpi_iter->iterator);
    errno = save_errno;
    return NULL;
}

    /*--------------------------------------------------------------------*
     |                          branch()                                  |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
lustre_mpi_branch_backend_filter(void *backend, const struct rbh_filter *filter,
                                 const struct rbh_filter_options *options)
{
    struct posix_branch_backend *branch = backend;
    struct mpi_iterator *mpi_iter;
    char *root, *path;
    int save_errno;

    if (filter != NULL) {
        errno = ENOTSUP;
        return NULL;
    }

    if (options->skip > 0 || options->limit > 0 || options->sort.count > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    root = realpath(branch->posix.root, NULL);
    if (root == NULL)
        return NULL;

    if (branch->path) {
        path = branch->path;
    } else {
        path = id2path(root, &branch->id);
        save_errno = errno;
        if (path == NULL) {
            free(root);
            errno = save_errno;
            return NULL;
        }
    }

    assert(strncmp(root, path, strlen(root)) == 0);
    mpi_iter = (struct mpi_iterator *)
                branch->posix.iter_new(root, path + strlen(root),
                                       branch->posix.statx_sync_type);
    mpi_iter->skip_error = options->skip_error;
    mpi_iter->is_branch = true;
    save_errno = errno;
    free(path);
    free(root);
    errno = save_errno;

    return (struct rbh_mut_iterator *)mpi_iter;
}

static const struct rbh_backend_operations LUSTRE_MPI_BRANCH_BACKEND_OPS = {
    .root = posix_root,
    .branch = lustre_mpi_backend_branch,
    .filter = lustre_mpi_branch_backend_filter,
    .destroy = posix_backend_destroy,
};

static const struct rbh_backend LUSTRE_MPI_BRANCH_BACKEND = {
    .name = RBH_LUSTRE_MPI_BACKEND_NAME,
    .ops = &LUSTRE_MPI_BRANCH_BACKEND_OPS,
};

struct rbh_backend *
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
    .get_option = posix_backend_get_option,
    .set_option = posix_backend_set_option,
    .branch = lustre_mpi_backend_branch,
    .root = posix_root,
    .filter = lustre_mpi_backend_filter,
    .get_attribute = lustre_backend_get_attribute,
    .destroy = posix_backend_destroy,
};

struct rbh_backend *
rbh_lustre_mpi_backend_new(const char *path)
{
    struct posix_backend *lustre_mpi;

    lustre_mpi = (struct posix_backend *)rbh_posix_backend_new(path);
    if (lustre_mpi == NULL)
        return NULL;

    lustre_mpi->iter_new = lustre_mpi_iterator_new;
    lustre_mpi->backend.name = RBH_LUSTRE_MPI_BACKEND_NAME;
    lustre_mpi->backend.ops = &LUSTRE_MPI_BACKEND_OPS;
    lustre_mpi->backend.id = RBH_BI_LUSTRE_MPI;

    return &lustre_mpi->backend;
}
