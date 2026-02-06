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
#include <robinhood/utils.h>

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

static struct rbh_id *
_build_id(const char *path, bool is_mpifile)
{
    struct rbh_id *id;
    int fd;

    if (is_mpifile) {
        id = rbh_id_new_with_id(path, (strlen(path) + 1) * sizeof(*path),
                                RBH_BI_MPI_FILE);
    } else {
        fd = openat(AT_FDCWD, path,
                    O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
        if (fd < 0)
            return NULL;

        id = id_from_fd(fd, RBH_BI_POSIX);
        close(fd);
    }

    return id;
}

struct rbh_id *
mfu_build_parent_id(const char *accpath, size_t prefix_len, bool is_mpifile)
{
    struct rbh_id *id;
    const char *path;
    char *tmp;

    tmp = xstrdup(accpath);

    if (is_mpifile)
        path = dirname(tmp);
    else
        path = dirname(strlen(tmp) == prefix_len ? tmp : tmp + prefix_len);

    id = _build_id(path, is_mpifile);
    free(tmp);

    return id;
}

struct rbh_id *
mfu_build_id(const char *accpath, size_t prefix_len, bool is_mpifile)
{
    const char *path;

    if (is_mpifile)
        path = strlen(accpath) == prefix_len ? "/" : accpath + prefix_len;
    else
        path = accpath;

    return _build_id(path, is_mpifile);
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
