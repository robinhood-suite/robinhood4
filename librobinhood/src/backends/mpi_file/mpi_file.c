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

    if (statxbuf->stx_mask & RBH_STATX_MODE)
        mfu_flist_file_set_mode(flist, idx, statxbuf->stx_mode);

    if (statxbuf->stx_mask & RBH_STATX_UID)
        mfu_flist_file_set_uid(flist, idx, statxbuf->stx_uid);

    if (statxbuf->stx_mask & RBH_STATX_GID)
        mfu_flist_file_set_gid(flist, idx, statxbuf->stx_gid);

    /* ATIME */
    if (statxbuf->stx_mask & RBH_STATX_ATIME) {
        if (statxbuf->stx_mask & RBH_STATX_ATIME_SEC)
            mfu_flist_file_set_atime(flist, idx, statxbuf->stx_atime.tv_sec);
        if (statxbuf->stx_mask & RBH_STATX_ATIME_NSEC)
            mfu_flist_file_set_atime_nsec(flist, idx,
                                          statxbuf->stx_atime.tv_nsec);
    }

    /* MTIME */
    if (statxbuf->stx_mask & RBH_STATX_MTIME) {
        if (statxbuf->stx_mask & RBH_STATX_MTIME_SEC)
            mfu_flist_file_set_mtime(flist, idx, statxbuf->stx_mtime.tv_sec);
        if (statxbuf->stx_mask & RBH_STATX_MTIME_NSEC)
            mfu_flist_file_set_mtime_nsec(flist, idx,
                                          statxbuf->stx_mtime.tv_nsec);
    }

    /* CTIME */
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

static uint64_t
flist_append_upsert(mfu_flist flist, const struct rbh_statx *statxbuf)
{
    mfu_filetype type;
    uint64_t index;

    index = mfu_flist_file_create(flist);

    if (statxbuf) {
        /* Set filetype */
        type = mfu_flist_mode_to_filetype(statxbuf->stx_mode);
        mfu_flist_file_set_type(flist, index, type);

        /* Set statx */
        mfu_flist_file_set_detail(flist, index, 1);
        _mfu_flist_file_set_statx(flist, index, statxbuf);
    }

    return index;
}

static uint64_t
flist_append_inode_xattr(mfu_flist flist)
{
    return mfu_flist_file_create(flist);
}


static uint64_t
flist_append_link(mfu_flist flist, uint64_t *idx,
                  const struct rbh_value_map *xattrs)
{
    const char *path;
    uint64_t index;

    /* Set file path */
    path = map_get_path(xattrs);
    if (path == NULL)
        return -1;

    /**
     * If idx != NULL, we modify the element
     * If idx == NULL, we insert a new element in the flist and return the
     * new index
     */
    if (idx != NULL) {
        mfu_flist_file_set_name(flist, *idx, path);

        return *idx;
    } else {
        index = mfu_flist_file_create(flist);
        mfu_flist_file_set_name(flist, index, path);

        return index;
    }
}

static uint64_t
flist_append_ns_xattr(mfu_flist flist, uint64_t *idx,
                      const struct rbh_value_map *xattrs)
{
    return flist_append_link(flist, idx, xattrs);
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

/* Index in the flist of the last entry inserted */
static uint64_t last_idx;
/* rbh_id of the last entry inserted in the flist */
static struct rbh_id *last_id;

static bool
mpi_file_update_flist(mfu_flist flist, const struct rbh_fsevent *fsevent)
{
    switch (fsevent->type) {
    case RBH_FET_UPSERT:
        last_idx = flist_append_upsert(flist, fsevent->upsert.statx);
        last_id = (struct rbh_id *) &fsevent->id;
        break;
    case RBH_FET_LINK:
        if (&fsevent->id == last_id) {
            /* In this case, we update the last entry in the flist with the
             * information in the fsevent
             */
            last_idx = flist_append_link(flist, &last_idx, &fsevent->xattrs);
        } else {
            /* Otherwise, we create a new entry in the flist and store his
             * rbh_id
             */
            last_idx = flist_append_link(flist, NULL, &fsevent->xattrs);
            last_id = (struct rbh_id *) &fsevent->id;
        }
        break;
    case RBH_FET_XATTR:
        if (fsevent->ns.parent_id == NULL) {
            last_idx = flist_append_inode_xattr(flist);
            last_id = (struct rbh_id *) &fsevent->id;
        } else {
            if (&fsevent->id == last_id) {
                /* Same as RBH_FET_LINK */
                last_idx = flist_append_ns_xattr(flist, &last_idx,
                                                 &fsevent->xattrs);
            } else {
                /* Same as RBH_FET_LINK */
                last_idx = flist_append_ns_xattr(flist, NULL,
                                                 &fsevent->xattrs);
                last_id = (struct rbh_id *) &fsevent->id;
            }
        }
        break;
    default:
        return false;
    }

    if (last_idx == -1)
        return false;
    return true;
}

static ssize_t
mpi_file_backend_update(void *backend, struct rbh_iterator *fsevents,
                        bool skip_error)
{
    struct mpi_file_backend *mpi_file = backend;
    int save_errno = errno;
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

        if(!mpi_file_update_flist(mpi_file->flist, fsevent))
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
