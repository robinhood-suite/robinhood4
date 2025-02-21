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
#include "robinhood/backend.h"
#include "robinhood/statx.h"
#include "mpi_file.h"

/*----------------------------------------------------------------------------*
 |                          mpi_file iterator                                 |
 *----------------------------------------------------------------------------*/

static void
flist_file2statx(mfu_flist flist, uint64_t idx, struct rbh_statx *statxbuf)
{
    statxbuf->stx_mask = RBH_STATX_MPIFILE;

    statxbuf->stx_mode = mfu_flist_file_get_mode(flist, idx);
    statxbuf->stx_uid = mfu_flist_file_get_uid(flist, idx);
    statxbuf->stx_gid = mfu_flist_file_get_gid(flist, idx);

    statxbuf->stx_atime.tv_sec = mfu_flist_file_get_atime(flist, idx);
    statxbuf->stx_atime.tv_nsec = mfu_flist_file_get_atime_nsec(flist, idx);

    statxbuf->stx_mtime.tv_sec = mfu_flist_file_get_mtime(flist, idx);
    statxbuf->stx_mtime.tv_nsec = mfu_flist_file_get_mtime_nsec(flist, idx);

    statxbuf->stx_ctime.tv_sec = mfu_flist_file_get_ctime(flist, idx);
    statxbuf->stx_ctime.tv_nsec = mfu_flist_file_get_ctime_nsec(flist, idx);

    statxbuf->stx_size = mfu_flist_file_get_size(flist, idx);
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
    unsigned short bc_id = RBH_BI_MPI_FILE;
    char *symlink = NULL;
    struct rbh_id *id;
    int save_errno;

    const char *path = strlen(mpi_fi->path) == iterator->prefix_len ?
                        "/" : mpi_fi->path + iterator->prefix_len;
    /**
     * Unlike with posix, we use the relative path of an entry to
     * create an unique ID
     */
    size_t size = (strlen(mpi_fi->path) + 1) * sizeof(*mpi_fi->path);
    char data[size];
    char *tmp;
    tmp = mempcpy(data, &bc_id, sizeof(short));
    memcpy(tmp, mpi_fi->path, size);
    id = rbh_id_new(mpi_fi->path, size + sizeof(short));
    if (id == NULL) {
        save_errno = errno;
        return NULL;
    }

    flist_file2statx(iterator->flist, iterator->current, &statxbuf);

    if (S_ISLNK(statxbuf.stx_mode)) {
        static_assert(sizeof(size_t) == sizeof(statxbuf.stx_size), "");
        // We keep the asbolute path here to read the symbolic link
        symlink = freadlink(-1, mpi_fi->path, (size_t *)&statxbuf.stx_size);

        if (symlink == NULL) {
            fprintf(stderr, "Failed to readlink '%s': %s (%d)\n", path,
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
    ns_pairs->value = rbh_value_string_new(path);
    if (ns_pairs->value == NULL) {
        save_errno = errno;
        goto out_free_ns_pairs;
    }

    ns_xattrs.count = 1;
    ns_xattrs.pairs = ns_pairs;

    inode_xattrs.pairs = NULL;
    inode_xattrs.count = 0;

    fsentry = rbh_fsentry_new(id, mpi_fi->parent_id, mpi_fi->name, &statxbuf,
                              &ns_xattrs, &inode_xattrs, symlink);
    if (fsentry == NULL) {
        save_errno = errno;
        goto out_free_ns_pairs_value;
    }

    free((struct rbh_value *) ns_pairs->value);
    free(ns_pairs);
    free(symlink);
    free(id);

    return fsentry;

out_free_ns_pairs_value:
    free((struct rbh_value *) ns_pairs->value);
out_free_ns_pairs:
    free(ns_pairs);
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
mpi_file_iterator_new(mfu_flist flist, int prefix_len)
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
    mpi_file_iter->mpi_build_fsentry = fsentry_from_flist;
    mpi_file_iter->prefix_len = prefix_len;

    return (struct rbh_mut_iterator *)mpi_file_iter;
}

/*----------------------------------------------------------------------------*
 |                          flist_append_fsevent                              |
 *----------------------------------------------------------------------------*/

static void
_mfu_flist_file_set_statx(mfu_flist flist, uint64_t idx,
                          const struct rbh_statx *statxbuf)
{

    if ((statxbuf->stx_mask & RBH_STATX_MODE) &&
        (statxbuf->stx_mask & RBH_STATX_TYPE)) {
        mfu_flist_file_set_mode(flist, idx, statxbuf->stx_mode);
        mfu_flist_file_set_type(flist, idx,
                                mfu_flist_mode_to_filetype(statxbuf->stx_mode));
    }

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

    mfu_pred_times *now;
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
mpi_file_backend_update(void *backend, struct rbh_iterator *fsevents)
{
    struct rbh_id *last_id = rbh_id_new(NULL, 0);
    struct mpi_file_backend *mpi_file = backend;
    unsigned short bc_id = RBH_BI_MPI_FILE;
    int save_errno = errno;
    uint64_t index = 0;
    ssize_t count = 0;

    if (fsevents == NULL) {
        mfu_flist_summarize(mpi_file->flist);
        mfu_flist_write_cache(mpi_file->path, mpi_file->flist);
        free(last_id);
        return 0;
    }

    do {
        const struct rbh_fsevent *fsevent;

        fsevent = rbh_iter_next(fsevents);
        if (fsevent == NULL) {
            if (errno == ENODATA || errno == ENOTCONN)
                break;

            if (errno == ESTALE || errno == ENOENT ||
                errno == RBH_BACKEND_ERROR)
                continue;
            return -1;
        }

        if (!rbh_id_equal(last_id, &fsevent->id)) {
            index = mfu_flist_file_create(mpi_file->flist);
            char data[fsevent->id.size + sizeof(short)];
            char *tmp;
            tmp = mempcpy(data, &bc_id, sizeof(short));
            memcpy(tmp, fsevent->id.data, fsevent->id.size);
            size_t size = fsevent->id.size + sizeof(short);
            struct rbh_id *id = rbh_id_new(data, size);
            free(last_id);
            last_id = id;
        }

        if(!mpi_file_update_flist(mpi_file->flist, index, fsevent))
            return -1;
        count++;
    } while (true);

    free(last_id);
    errno = save_errno;
    return count;
}

    /*--------------------------------------------------------------------*
     |                          filter()                                  |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
mpi_file_backend_filter(
    void *backend, const struct rbh_filter *filter,
    const struct rbh_filter_options *options,
    __attribute__((unused)) const struct rbh_filter_output *output)
{
    struct mpi_file_backend *mpi_file = backend;
    struct mpi_iterator *mpi_file_iter;
    mfu_pred *pred_head;
    const char *root;
    int prefix_len;
    int rank;

    if (rbh_filter_validate(filter))
        return NULL;

    if (options->skip > 0 || options->limit > 0 || options->sort.count > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    mfu_flist_read_cache(mpi_file->path, mpi_file->flist);

    if (!mfu_flist_global_size(mpi_file->flist)) {
        errno = ENOENT;
        return NULL;
    }

    /**
     * We need to broadcast to all ranks the length of the root in order to
     * remove it from the path
     */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
        root = mfu_flist_file_get_name(mpi_file->flist, 0);
        prefix_len = strcmp(root, "/") ? strlen(root) : 0;
    }
    MPI_Bcast(&prefix_len, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (filter != NULL) {
        pred_head = rbh_filter2mfu_pred(filter, prefix_len, mpi_file->now);
        if (pred_head == NULL)
            return NULL;

        mfu_flist flist = mfu_flist_filter_pred(mpi_file->flist, pred_head);
        mfu_flist_free(&mpi_file->flist);
        _mfu_pred_free(pred_head);
        mpi_file->flist = flist;
    }

    mpi_file_iter = (struct mpi_iterator *)
                     mpi_file_iterator_new(mpi_file->flist, prefix_len);
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

    mfu_free(&mpi_file->now);
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

    mpi_file->now = mfu_pred_now();

    if (mpi_file->flist == NULL)
        error(EXIT_FAILURE, errno, "malloc flist");
}

struct rbh_backend *
rbh_mpi_file_backend_new(const char *path,
                         __attribute__((unused)) struct rbh_config *config)
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
