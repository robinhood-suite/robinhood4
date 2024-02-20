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
#include "robinhood/fsentry.h"
#include "robinhood/iterator.h"
#include "robinhood/id.h"
#include "robinhood/statx.h"

/*----------------------------------------------------------------------------*
 |                                mfu_iterator                                |
 *----------------------------------------------------------------------------*/

static struct rbh_id *
get_parent_id(const char *path)
{
    struct rbh_id *parent_id;
    int save_errno = errno;
    char *parent_path;
    char *tmp_path;
    int fd;

    tmp_path    = strdup(path);
    parent_path = dirname(tmp_path);

    fd = openat(AT_FDCWD, parent_path, O_RDONLY | O_CLOEXEC
                                       | O_PATH | O_NOFOLLOW);
    if (fd < 0) {
        save_errno = errno;
        free(tmp_path);
        errno = save_errno;
        return NULL;
    }

    parent_id = id_from_fd(fd);
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

    /**
     * Tell mpifileutils not to do stats during the walk
     */
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
    const char *path;
    char *path_dup;

skip:
    if (mpi_iter->current == mpi_iter->total) {
        errno = ENODATA;
        return NULL;
    }

    path = mfu_flist_file_get_name(mpi_iter->flist, mpi_iter->current);
    path_dup = strdup(path);

    struct mpi_file_info mpi_fi = {
        .path = path,
        .name = basename(path_dup),
        .parent_id = get_parent_id(path),
    };

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

    mpi_iter->flist = walk_path(path);
    mpi_iter->prefix_len = strcmp(root, "/") ? strlen(root) : 0;
    mpi_iter->total = mfu_flist_size(mpi_iter->flist);
    mpi_iter->statx_sync_type = statx_sync_type;
    mpi_iter->inode_xattrs_callback = NULL;
    mpi_iter->iterator = LUSTRE_MPI_ITER;
    mpi_iter->current = 0;

    free(path);

    return mpi_iter;
}
