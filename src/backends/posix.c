/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <fts.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include "robinhood/backends/posix.h"
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

struct posix_iterator {
    struct rbh_mut_iterator iterator;
    FTS *fts_handle;
};

static __thread size_t handle_size = MAX_HANDLE_SZ;
static __thread struct file_handle *handle;

static const struct rbh_id ROOT_PARENT_ID = {
    .data = NULL,
    .size = 0,
};

static struct rbh_id *
id_from_fd(int fd)
{
    struct rbh_id *id;
    int mount_id;

    if (handle == NULL) {
        /* Per-thread initialization of `handle' */
        handle = malloc(sizeof(*handle) + handle_size);
        if (handle == NULL)
            return NULL;
        handle->handle_bytes = handle_size;
    }

retry:
    handle->handle_bytes = handle_size;
    if (name_to_handle_at(fd, "", handle, &mount_id, AT_EMPTY_PATH)) {
        struct file_handle *tmp;

        if (errno != EOVERFLOW || handle->handle_bytes <= handle_size)
            return NULL;

        tmp = malloc(sizeof(*tmp) + handle->handle_bytes);
        if (tmp == NULL)
            return NULL;

        handle_size = handle->handle_bytes;
        free(handle);
        handle = tmp;
        goto retry;
    }

    id = malloc(sizeof(*id) + sizeof(handle->handle_type)
                + handle->handle_bytes);
    if (id == NULL)
        return NULL;
    id->data = (char *)id + sizeof(*id);
    id->size = sizeof(handle->handle_type) + handle->handle_bytes;

    memcpy(id->data, &handle->handle_type, sizeof(handle->handle_type));
    memcpy(id->data + sizeof(handle->handle_type), handle->f_handle,
           handle->handle_bytes);

    return id;
}

#ifndef HAVE_STATX
static int
statx_timestamp_from_timespec(struct statx_timestamp *timestamp,
                              struct timespec *timespec)
{
    if (timespec->tv_sec > INT64_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    timestamp->tv_sec = timespec->tv_sec;
    timestamp->tv_nsec = timespec->tv_nsec;
    return 0;
}

static void
statx_from_stat(struct statx *statxbuf, struct stat *stat)
{
    statxbuf->stx_mask = STATX_BASIC_STATS;
    statxbuf->stx_blksize = stat->st_blksize;
    statxbuf->stx_nlink = stat->st_nlink;
    statxbuf->stx_uid = stat->st_uid;
    statxbuf->stx_gid = stat->st_gid;
    statxbuf->stx_mode = stat->st_mode;
    statxbuf->stx_ino = stat->st_ino;
    statxbuf->stx_size = stat->st_size;
    statxbuf->stx_blocks = stat->st_blocks;
    statxbuf->stx_attributes_mask = 0;

    if (statx_timestamp_from_timespec(&statxbuf->stx_atime, &stat->st_atim))
        statxbuf->stx_mask &= ~STATX_ATIME;

    if (statx_timestamp_from_timespec(&statxbuf->stx_mtime, &stat->st_mtim))
        statxbuf->stx_mask &= ~STATX_MTIME;

    if (statx_timestamp_from_timespec(&statxbuf->stx_ctime, &stat->st_ctim))
        statxbuf->stx_mask &= ~STATX_CTIME;

    statxbuf->stx_rdev_major = major(stat->st_rdev);
    statxbuf->stx_rdev_minor = minor(stat->st_rdev);
    statxbuf->stx_dev_major = major(stat->st_dev);
    statxbuf->stx_dev_minor = minor(stat->st_dev);
}
#endif

/* XXX: this wrapper is only needed for as long as we support platforms where
 *      statx() is not defined
 */
static int
_statx(int dirfd, const char *pathname, int flags, unsigned int mask,
       struct statx *statxbuf)
{
#ifdef HAVE_STATX
    return statx(dirfd, pathname, flags, mask, statxbuf);
#else
    struct stat stat;

    /* `flags' may contain values specific to statx, let's remove them */
    if (fstatat(dirfd, pathname, &stat, flags & ~AT_STATX_SYNC_TYPE))
        return -1;

    statx_from_stat(statxbuf, &stat);
    return 0;
#endif
}

static char *
freadlink(int fd, size_t *size_)
{
    size_t size = *size_ + 1;
    char *symlink;
    ssize_t rc;

retry:
    symlink = malloc(size);
    if (symlink == NULL)
        return NULL;

    rc = readlinkat(fd, "", symlink, size);
    if (rc < 0) {
        int save_errno = errno;

        free(symlink);
        errno = save_errno;
        return NULL;
    }
    if (rc == size) {
        /* Output may have been truncated, try a bigger size to check */
        free(symlink);

        /* We do not need to worry much about memory consumption, the VFS caps
         * the size of an extended attribute at 64kB.
         *
         * But just to be sure...
         */
        if (size >= (1 << 16)) { /* size >= 64kB */
            errno = EOVERFLOW;
            return NULL;
        }
        size *= 2;

        goto retry;
    }
    /* readlinkat does not append the null terminating byte */
    symlink[rc] = '\0';
    *size_ = rc;

    return symlink;
}

static long page_size;
static void get_page_size(void) __attribute__((constructor));

static void
get_page_size(void)
{
    page_size = sysconf(_SC_PAGESIZE);
}

static struct rbh_fsentry *
fsentry_from_ftsent(FTSENT *ftsent, int statx_sync_type)
{
    const int statx_flags =
        AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT;
    struct rbh_fsentry *fsentry;
    struct statx statxbuf;
    struct rbh_id *id;
    char *symlink = NULL;
    int save_errno;
    int fd;

    fd = openat(AT_FDCWD, ftsent->fts_accpath,
                O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_PATH);
    if (fd < 0)
        return NULL;

    id = id_from_fd(fd);
    if (id == NULL) {
        save_errno = errno;
        goto out_close;
    }

    if (_statx(fd, "", statx_flags | statx_sync_type, STATX_ALL, &statxbuf)) {
        save_errno = errno;
        goto out_close;
    }

    /* We want the actual type of the file we opened, not the one fts saw */
    if (statxbuf.stx_mask & STATX_TYPE && S_ISLNK(statxbuf.stx_mode)) {
        if ((statxbuf.stx_mask & STATX_SIZE) == 0) {
            statxbuf.stx_size = page_size - 1;
            statxbuf.stx_mask |= STATX_SIZE;
        }
        symlink = freadlink(fd, &statxbuf.stx_size);

        if (symlink == NULL) {
            save_errno = errno;
            goto out_free_id;
        }
    }

    if (ftsent->fts_parent->fts_pointer == NULL) /* ftsent == root */
        fsentry = rbh_fsentry_new(id, &ROOT_PARENT_ID, "", &statxbuf, symlink);
    else
        fsentry = rbh_fsentry_new(id, ftsent->fts_parent->fts_pointer,
                                  ftsent->fts_name, &statxbuf, symlink);
    if (fsentry == NULL) {
        save_errno = errno;
        goto out_free_symlink;
    }

    free(symlink);
    /* Ignore errors on close */
    close(fd);

    switch (ftsent->fts_info) {
    case FTS_D:
        /* memoize ids of directories */
        ftsent->fts_pointer = id;
        break;
    default:
        free(id);
    }

    return fsentry;

out_free_symlink:
    free(symlink);
out_free_id:
    free(id);
out_close:
    close(fd);

    errno = save_errno;
    return NULL;
}

static void *
posix_iter_next(void *iterator)
{
    struct posix_iterator *posix_iter = iterator;
    struct rbh_fsentry *fsentry;
    FTSENT *ftsent;
    int save_errno = errno;

skip:
    errno = 0;
    ftsent = fts_read(posix_iter->fts_handle);
    if (ftsent == NULL) {
        errno = errno ? : ENODATA;
        return NULL;
    }
    errno = save_errno;

    switch (ftsent->fts_info) {
    case FTS_DP:
        /* fsentry_from_ftsent() memoizes ids of directories */
        free(ftsent->fts_pointer);
        goto skip;
    case FTS_DC:
        errno = ELOOP;
        return NULL;
    case FTS_DNR:
    case FTS_ERR:
    case FTS_NS:
        errno = ftsent->fts_errno;
        return NULL;
    }

    fsentry = fsentry_from_ftsent(ftsent, AT_STATX_SYNC_AS_STAT);
    if (fsentry == NULL && (errno == ENOENT || errno == ESTALE))
        /* The entry moved from under our feet */
        goto skip;

    return fsentry;
}

static void
posix_iter_destroy(void *iterator)
{
    struct posix_iterator *posix_iter = iterator;

    fts_close(posix_iter->fts_handle);
    free(posix_iter);
}

static const struct rbh_mut_iterator_operations POSIX_ITER_OPS = {
    .next = posix_iter_next,
    .destroy = posix_iter_destroy,
};

static const struct rbh_mut_iterator POSIX_ITER = {
    .ops = &POSIX_ITER_OPS,
};

struct posix_backend {
    struct rbh_backend backend;
    char *root;
};

static struct rbh_mut_iterator *
posix_backend_filter_fsentries(void *backend, const struct rbh_filter *filter,
                               unsigned int fsentries_mask,
                               unsigned int statx_mask)
{
    struct posix_backend *posix = backend;
    char *paths[] = {posix->root, NULL};
    struct posix_iterator *posix_iter;

    /* TODO: make use of `fsentries_mask' and `statx_mask' */

    if (filter != NULL) {
        errno = ENOTSUP;
        return NULL;
    }

    posix_iter = malloc(sizeof(*posix_iter));
    if (posix_iter == NULL)
        return NULL;

    posix_iter->fts_handle =
        fts_open(paths, FTS_PHYSICAL | FTS_NOSTAT | FTS_XDEV, NULL);
    if (posix_iter->fts_handle == NULL) {
        int save_errno = errno;

        free(posix_iter);
        errno = save_errno;
        return NULL;
    }

    posix_iter->iterator = POSIX_ITER;

    return &posix_iter->iterator;
}

static void
posix_backend_destroy(void *backend)
{
    struct posix_backend *posix = backend;

    free(posix->root);
    free(posix);
}

static const struct rbh_backend_operations POSIX_BACKEND_OPS = {
    .filter_fsentries = posix_backend_filter_fsentries,
    .destroy = posix_backend_destroy,
};

static const struct rbh_backend POSIX_BACKEND = {
    .name = RBH_POSIX_BACKEND_NAME,
    .ops = &POSIX_BACKEND_OPS,
};

struct rbh_backend *
rbh_posix_backend_new(const char *path)
{
    struct posix_backend *posix;

    posix = malloc(sizeof(*posix));
    if (posix == NULL)
        return NULL;

    posix->root = strdup(path);
    if (posix->root == NULL) {
        int save_errno = errno;

        free(posix);
        errno = save_errno;
        return NULL;
    }

    posix->backend = POSIX_BACKEND;

    return &posix->backend;
}
