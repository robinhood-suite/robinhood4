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
#include <assert.h>
#include <libgen.h>

#include "mfu.h"
#include "robinhood/backends/posix_internal.h"
#include "robinhood/backends/iter_mpi_internal.h"
#include "robinhood/backends/mpi_file.h"
#include "robinhood/statx.h"

/*----------------------------------------------------------------------------*
 |                          mpi_file iterator                                 |
 *----------------------------------------------------------------------------*/

static void
flist_file2statx(mfu_flist flist, uint64_t idx, struct rbh_statx *statxbuf)
{
    statxbuf->stx_mask = RBH_STATX_EMPTY;
    uint64_t ret;

    /* MODE */
    ret = mfu_flist_file_get_mode(flist, idx);
    statxbuf->stx_mode = ret;
    statxbuf->stx_mask |= RBH_STATX_MODE;
    statxbuf->stx_mask |= RBH_STATX_TYPE;

    /* UID */
    ret = mfu_flist_file_get_uid(flist, idx);
    statxbuf->stx_uid = ret;
    statxbuf->stx_mask |= RBH_STATX_UID;

    /* GID */
    ret = mfu_flist_file_get_gid(flist, idx);
    statxbuf->stx_gid = ret;
    statxbuf->stx_mask |= RBH_STATX_GID;

    /* ATIME */
    ret = mfu_flist_file_get_atime(flist, idx);
    if (ret != -1) {
        statxbuf->stx_atime.tv_sec = ret;
        statxbuf->stx_mask |= RBH_STATX_ATIME_SEC;
    }

    ret = mfu_flist_file_get_atime_nsec(flist, idx);
    if (ret != -1) {
        statxbuf->stx_atime.tv_nsec = ret;
        statxbuf->stx_mask |= RBH_STATX_ATIME_NSEC;
    }

    /* MTIME */
    ret = mfu_flist_file_get_mtime(flist, idx);
    if (ret != -1) {
        statxbuf->stx_mtime.tv_sec = ret;
        statxbuf->stx_mask |= RBH_STATX_MTIME_SEC;
    }

    ret = mfu_flist_file_get_mtime_nsec(flist, idx);
    if (ret != -1) {
        statxbuf->stx_mtime.tv_nsec = ret;
        statxbuf->stx_mask |= RBH_STATX_MTIME_NSEC;
    }

    /* CIMTE */
    ret = mfu_flist_file_get_ctime(flist, idx);
    if (ret != -1) {
        statxbuf->stx_ctime.tv_sec = ret;
        statxbuf->stx_mask |= RBH_STATX_CTIME_SEC;
    }

    ret = mfu_flist_file_get_ctime_nsec(flist, idx);
    if (ret != -1) {
        statxbuf->stx_ctime.tv_nsec = ret;
        statxbuf->stx_mask |= RBH_STATX_CTIME_NSEC;
    }

    /* SIZE */
    ret = mfu_flist_file_get_size(flist, idx);
    if (ret != -1) {
        statxbuf->stx_size = ret;
        statxbuf->stx_mask |= RBH_STATX_SIZE;
    }
}

static struct rbh_fsentry *
fsentry_from_flist(struct mpi_file_info *mpi_fi,
                   struct mpi_iterator *iterator)
{
    struct rbh_value_map inode_xattrs;
    struct rbh_value_pair *ns_pairs;
    struct rbh_value_map ns_xattrs;
    struct rbh_fsentry *fsentry;
    struct rbh_statx statxbuf;
    char *symlink = NULL;
    struct rbh_id *id;
    int save_errno;
    /**
     * Unlike with posix, we use the relative path of an entry to
     * create an unique ID
     */
    id = rbh_id_new(mpi_fi->path, (strlen(mpi_fi->path) + 1) * sizeof(char));
    if (id == NULL) {
        save_errno = errno;
        return NULL;
    }

    flist_file2statx(iterator->flist, iterator->current, &statxbuf);

    if (statxbuf.stx_mask & RBH_STATX_TYPE && S_ISLNK(statxbuf.stx_mode)) {
        static_assert(sizeof(size_t) == sizeof(statxbuf.stx_size), "");
        symlink = freadlink(-1, mpi_fi->path, (size_t *)&statxbuf.stx_size);

        if (symlink == NULL) {
            fprintf(stderr, "Failed to readlink '%s': %s (%d)\n", mpi_fi->path,
                    strerror(errno), errno);
            errno = ESTALE;
            save_errno = errno;
            goto out_free_id;
        }
    }

    ns_pairs = malloc(sizeof(*ns_pairs));
    if (ns_pairs == NULL) {
        save_errno = errno;
        goto out_free_symlink;
    }
    ns_pairs->key = "path";
    ns_pairs->value = rbh_value_string_new(mpi_fi->path);
    if (ns_pairs->value == NULL) {
        save_errno = errno;
        goto out_free_symlink;
    }

    ns_xattrs.count = 1;
    ns_xattrs.pairs = ns_pairs;

    inode_xattrs.pairs = NULL;
    inode_xattrs.count = 0;

    fsentry = rbh_fsentry_new(id, mpi_fi->parent_id, mpi_fi->name, &statxbuf,
                              &ns_xattrs, &inode_xattrs, symlink);
    if (fsentry == NULL) {
        save_errno = errno;
        goto out_free_symlink;
    }

    free(symlink);

    return fsentry;

out_free_symlink:
    free(symlink);
out_free_id:
    free(id);

    errno = save_errno;
    return NULL;

}

static void
mpi_file_iter_destroy(void *iterator)
{
    struct mpi_iterator *mpi_file_iter = iterator;

    free(mpi_file_iter);
}

static const struct rbh_mut_iterator_operations MPI_FILE_ITER_OPS = {
    .next = mpi_iter_next,
    .destroy = mpi_file_iter_destroy,
};

static const struct rbh_mut_iterator MPI_FILE_ITER = {
    .ops = &MPI_FILE_ITER_OPS,
};

static struct rbh_mut_iterator *
mpi_file_iterator_new(mfu_flist flist)
{
    struct mpi_iterator *mpi_file_iter;

    mpi_file_iter = malloc(sizeof(*mpi_file_iter));
    if (mpi_file_iter == NULL)
        error(EXIT_FAILURE, errno, "malloc mpi_iterator");

    mpi_file_iter->iterator = MPI_FILE_ITER;
    mpi_file_iter->flist = flist;
    mpi_file_iter->current = 0;
    mpi_file_iter->total = mfu_flist_size(flist);
    mpi_file_iter->is_branch = false;
    mpi_file_iter->use_fd = false;
    mpi_file_iter->fsentry_from_mpi = fsentry_from_flist;

    return (struct rbh_mut_iterator *)mpi_file_iter;
}

/*----------------------------------------------------------------------------*
 |                          flist_append_fsevent                              |
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
     |                          filter()                                  |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
mpi_file_backend_filter(void *backend, const struct rbh_filter *filter,
                        const struct rbh_filter_options *options)
{
    struct mpi_file_backend *mpi_file = backend;
    struct mpi_iterator *mpi_file_iter;
    uint64_t global_size;

    if (filter != NULL) {
        errno = ENOTSUP;
        return NULL;
    }

    if (options->skip > 0 || options->limit > 0 || options->sort.count > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    mfu_flist_read_cache(mpi_file->path, mpi_file->flist);

    global_size = mfu_flist_global_size(mpi_file->flist);
    if (global_size == 0) {
        errno = ENOENT;
        return NULL;
    }

    mpi_file_iter = (struct mpi_iterator *)
                     mpi_file_iterator_new(mpi_file->flist);
    if (mpi_file_iter == NULL)
        return NULL;
    mpi_file_iter->skip_error = options->skip_error;

    return &mpi_file_iter->iterator;
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
    .filter = mpi_file_backend_filter,
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
