/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>
#include <errno.h>
#include <string.h>

#include "mfu.h"

#include "robinhood/backends/mpi_file.h"
#include "robinhood/statx.h"

/*----------------------------------------------------------------------------*
 |                          flist_append_fsevent                             |
 *----------------------------------------------------------------------------*/

static void
_mfu_flist_file_set_statx(mfu_flist flist, uint64_t idx,
                          const struct rbh_statx *statxbuf)
{

    if ((statxbuf->stx_mask & RBH_STATX_MODE) &&
        (statxbuf->stx_mask & RBH_STATX_TYPE))
        mfu_flist_file_set_mode(flist, idx, statxbuf->stx_mode);

    if (statxbuf->stx_mask & RBH_STATX_UID)
        mfu_flist_file_set_uid(flist, idx, statxbuf->stx_uid);

    if (statxbuf->stx_mask & RBH_STATX_GID)
        mfu_flist_file_set_gid(flist, idx, statxbuf->stx_gid);

    if (statxbuf->stx_mask & RBH_STATX_ATIME) {
        if (statxbuf->stx_mask & RBH_STATX_ATIME_SEC)
            mfu_flist_file_set_atime(flist, idx, statxbuf->stx_atime.tv_sec);
        if (statxbuf->stx_mask & RBH_STATX_ATIME_NSEC)
            mfu_flist_file_set_atime_nsec(flist, idx,
                                          statxbuf->stx_atime.tv_nsec);
    }

    if (statxbuf->stx_mask & RBH_STATX_MTIME) {
        if (statxbuf->stx_mask & RBH_STATX_MTIME_SEC)
            mfu_flist_file_set_mtime(flist, idx, statxbuf->stx_mtime.tv_sec);
        if (statxbuf->stx_mask & RBH_STATX_MTIME_NSEC)
            mfu_flist_file_set_mtime_nsec(flist, idx,
                                          statxbuf->stx_mtime.tv_nsec);
    }

    if (statxbuf->stx_mask & RBH_STATX_CTIME) {
        if (statxbuf->stx_mask & RBH_STATX_CTIME_SEC)
            mfu_flist_file_set_ctime(flist, idx, statxbuf->stx_ctime.tv_sec);
        if (statxbuf->stx_mask & RBH_STATX_CTIME_NSEC)
            mfu_flist_file_set_ctime_nsec(flist, idx,
                                          statxbuf->stx_ctime.tv_nsec);
    }

    if (statxbuf->stx_mask & RBH_STATX_SIZE)
        mfu_flist_file_set_size(flist, idx, statxbuf->stx_size);
}

static const char *
map_get_path(const struct rbh_value_map *map)
{
    for (size_t i = 0; i < map->count; i++) {
        const struct rbh_value_pair *pair = &map->pairs[i];

        if (strcmp(pair->key, "path") == 0) {
            const struct rbh_value *value = pair->value;

            if (value->type == RBH_VT_STRING)
                return value->string;

            return NULL;
        }
    }
    return NULL;
}

static bool
flist_append_upsert(mfu_flist flist, uint64_t index,
                    const struct rbh_statx *statxbuf)
{
    mfu_filetype type;

    if (statxbuf) {
        /* Set filetype */
        type = mfu_flist_mode_to_filetype(statxbuf->stx_mode);
        mfu_flist_file_set_type(flist, index, type);

        /* Set statx */
        mfu_flist_file_set_detail(flist, index, 1);
        _mfu_flist_file_set_statx(flist, index, statxbuf);
    }
    return true;
}

static bool
flist_append_link(mfu_flist flist, uint64_t index,
                  const struct rbh_value_map *xattrs)
{
    const char *path;

    /* Set file path */
    path = map_get_path(xattrs);
    if (path == NULL)
        return false;

    mfu_flist_file_set_name(flist, index, path);

    return true;
}

static bool
flist_append_ns_xattr(mfu_flist flist, uint64_t index,
                      const struct rbh_value_map *xattrs)
{
    return flist_append_link(flist, index, xattrs);
}

/*----------------------------------------------------------------------------*
 |                          mpi_file_backend                                  |
 *----------------------------------------------------------------------------*/

struct mpi_file_backend {
    struct rbh_backend backend;
    /*
     * Path of the mpi-file
     */
    const char *path;
    mfu_flist flist;
};

    /*--------------------------------------------------------------------*
     |                          update()                                  |
     *--------------------------------------------------------------------*/

static bool
mpi_file_update_flist(mfu_flist flist, uint64_t index,
                      const struct rbh_fsevent *fsevent)
{
    switch (fsevent->type) {
    case RBH_FET_UPSERT:
        return flist_append_upsert(flist, index, fsevent->upsert.statx);
    case RBH_FET_LINK:
        return flist_append_link(flist, index, &fsevent->xattrs);
    case RBH_FET_XATTR:
        if (fsevent->ns.parent_id)
            return flist_append_ns_xattr(flist, index, &fsevent->xattrs);
        return true;
    default:
        errno = EINVAL;
        return false;
    }
}

static ssize_t
mpi_file_backend_update(void *backend, struct rbh_iterator *fsevents,
                        bool skip_error)
{
    struct mpi_file_backend *mpi_file = backend;
    struct rbh_id last_id = {
        .data = NULL,
        .size = 0,
    };
    int save_errno = errno;
    uint64_t index = 0;
    ssize_t count = 0;

    if (fsevents == NULL) {
        mfu_flist_summarize(mpi_file->flist);
        mfu_flist_write_cache(mpi_file->path, mpi_file->flist);
        return 0;
    }

    do {
        const struct rbh_fsevent *fsevent;

        fsevent = rbh_iter_next(fsevents);
        if (fsevent == NULL) {
            if (errno == ENODATA || errno == ENOTCONN || !skip_error)
                break;

            if (errno == ESTALE || errno == ENOENT ||
                errno == RBH_BACKEND_ERROR)
                continue;
            return -1;
        }

        if (!rbh_id_equal(&last_id, &fsevent->id)) {
            index = mfu_flist_file_create(mpi_file->flist);
            struct rbh_id *id = rbh_id_new(fsevent->id.data, fsevent->id.size);
            last_id = *id;
        }

        if(!mpi_file_update_flist(mpi_file->flist, index, fsevent))
            return -1;
        count++;
    } while (true);

    errno = save_errno;
    return count;
}
    /*--------------------------------------------------------------------*
     |                          destroy()                                 |
     *--------------------------------------------------------------------*/

static void
mpi_file_backend_destroy(void *backend)
{
    struct mpi_file_backend *mpi_file = backend;

    mfu_flist_free(&mpi_file->flist);
    free((char *)mpi_file->path);
    free(mpi_file);
}

    /*--------------------------------------------------------------------*
     |                          backend()                                 |
     *--------------------------------------------------------------------*/

static const struct rbh_backend_operations MPI_FILE_BACKEND_OPS = {
    .update = mpi_file_backend_update,
    .destroy = mpi_file_backend_destroy,
};

static const struct rbh_backend MPI_FILE_BACKEND = {
    .id = RBH_BI_MPI_FILE,
    .name = RBH_MPI_FILE_BACKEND_NAME,
    .ops = &MPI_FILE_BACKEND_OPS,
};

static void
mpi_file_backend_init(struct mpi_file_backend *mpi_file)
{
    int flag;

    MPI_Initialized(&flag);
    if (!flag) {
        MPI_Init(NULL, NULL);
        mfu_init();
    }

    mpi_file->flist = mfu_flist_new();
    /* We tell mpifileutils that we have the stat informations */
    mfu_flist_set_detail(mpi_file->flist, 1);

    if (mpi_file->flist == NULL)
        error(EXIT_FAILURE, errno, "malloc flist");
}

struct rbh_backend *
rbh_mpi_file_backend_new(const char *path)
{
    struct mpi_file_backend *mpi_file;
    int save_errno;

    mpi_file = malloc(sizeof(*mpi_file));
    if (mpi_file == NULL)
        error(EXIT_FAILURE, errno, "malloc mpi_file_backend struct");

    if (*path == '\0') {
        save_errno = errno;
        free(mpi_file);
        errno = save_errno;
        return NULL;
    }

    mpi_file->path = strdup(path);
    if (mpi_file->path == NULL) {
        save_errno = errno;
        free(mpi_file);
        errno = save_errno;
        return NULL;
    }

    mpi_file_backend_init(mpi_file);
    mpi_file->backend = MPI_FILE_BACKEND;

    return &mpi_file->backend;
}
