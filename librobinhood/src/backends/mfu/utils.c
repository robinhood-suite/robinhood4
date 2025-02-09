/* This file is part of RobinHood 4
 *
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <errno.h>
#include <error.h>
#include <libgen.h>
#include <string.h>

#include <robinhood/backends/posix_extension.h>
#include <robinhood/id.h>

#include "mfu_internals.h"

mfu_flist
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

struct rbh_id *
get_parent_id(const char *path, bool use_fd, int prefix_len)
{
    struct rbh_id *parent_id;
    int save_errno = errno;
    char *parent_path;
    char *tmp_path;
    int fd;

    tmp_path = strdup(path);
    if (tmp_path == NULL)
        return NULL;

    if (use_fd) {
        parent_path = dirname(tmp_path);
        fd = openat(AT_FDCWD, parent_path, O_RDONLY | O_CLOEXEC | O_PATH);
        if (fd < 0) {
            save_errno = errno;
            free(tmp_path);
            errno = save_errno;
            return NULL;
        }

        parent_id = id_from_fd(fd);
    } else {
        parent_path = dirname(strlen(tmp_path) == prefix_len ? tmp_path :
                              tmp_path + prefix_len);
        parent_id = rbh_id_new(parent_path,
                               (strlen(parent_path) + 1) * sizeof(*parent_id));
    }

    save_errno = errno;
    if (use_fd)
        close(fd);
    free(tmp_path);
    errno = save_errno;

    return parent_id;
}

struct rbh_fsentry *
fsentry_from_fi(struct file_info *fi,
                struct posix_iterator *posix)
{
    int statx_sync_type = posix->statx_sync_type;
    int prefix_len = posix->prefix_len;
    const struct rbh_value path = {
        .type = RBH_VT_STRING,
        .string = strlen(fi->path) == prefix_len ? "/" : fi->path + prefix_len,
    };
    struct fsentry_id_pair pair;
    bool fsentry_success;

    fsentry_success = fsentry_from_any(&pair, &path, (char *)fi->path,
                                       NULL, fi->parent_id, fi->name,
                                       statx_sync_type,
                                       posix->enrichers);
    if (!fsentry_success)
        return NULL;

    return pair.fsentry;
}

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
