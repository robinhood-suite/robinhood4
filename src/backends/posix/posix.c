/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <fts.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include "robinhood/backends/posix.h"
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

/*----------------------------------------------------------------------------*
 |                               posix_iterator                               |
 *----------------------------------------------------------------------------*/

struct posix_iterator {
    struct rbh_mut_iterator iterator;
    int statx_sync_type;
    size_t prefix_len;
    FTS *fts_handle;
    FTSENT *ftsent;
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

    return rbh_id_from_file_handle(handle);
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
fsentry_from_ftsent(FTSENT *ftsent, int statx_sync_type, size_t prefix_len)
{
    const int statx_flags =
        AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT;
    const struct rbh_value PATH = {
        .type = RBH_VT_STRING,
        .string = ftsent->fts_pathlen == prefix_len ?
            "/" : ftsent->fts_path + prefix_len,
    };
    const struct rbh_value_pair PAIR = {
        .key = "path",
        .value = &PATH,
    };
    const struct rbh_value_map XATTRS = {
        .pairs = &PAIR,
        .count = 1,
    };
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

    /* The root entry might already have its ID computed and stored in
     * `fts_pointer'.
     */
    id = ftsent->fts_pointer ? : id_from_fd(fd);
    if (id == NULL) {
        save_errno = errno;
        goto out_close;
    }

    if (_statx(fd, "", statx_flags | statx_sync_type, STATX_ALL, &statxbuf)) {
        save_errno = errno;
        goto out_free_id;
    }

    /* We want the actual type of the file we opened, not the one fts saw */
    if (statxbuf.stx_mask & STATX_TYPE && S_ISLNK(statxbuf.stx_mode)) {
        if ((statxbuf.stx_mask & STATX_SIZE) == 0) {
            statxbuf.stx_size = page_size - 1;
            statxbuf.stx_mask |= STATX_SIZE;
        }
        static_assert(sizeof(size_t) == sizeof(statxbuf.stx_size), "");
        symlink = freadlink(fd, (size_t *)&statxbuf.stx_size);

        if (symlink == NULL) {
            save_errno = errno;
            goto out_free_id;
        }
    }

    fsentry = rbh_fsentry_new(id, ftsent->fts_parent->fts_pointer,
                              ftsent->fts_name, &statxbuf, &XATTRS, NULL,
                              symlink);
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
    posix_iter->ftsent = ftsent;

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

    fsentry = fsentry_from_ftsent(ftsent, posix_iter->statx_sync_type,
                                  posix_iter->prefix_len);
    if (fsentry == NULL && (errno == ENOENT || errno == ESTALE))
        /* The entry moved from under our feet */
        goto skip;

    return fsentry;
}

static void
posix_iter_destroy(void *iterator)
{
    struct posix_iterator *posix_iter = iterator;
    FTSENT *ftsent;

    while ((ftsent = fts_read(posix_iter->fts_handle)) != NULL) {
        switch (ftsent->fts_info) {
        case FTS_D:
            fts_set(posix_iter->fts_handle, ftsent, FTS_SKIP);
            break;
        case FTS_DP:
            /* fsentry_from_ftsent() memoizes ids of directories */
            free(ftsent->fts_pointer);
            break;
        }
    }
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

static struct posix_iterator *
posix_iterator_new(const char *root, const char *entry, int statx_sync_type)
{
    struct posix_iterator *posix_iter;
    char *paths[2] = {NULL, NULL};
    int save_errno;

    /* `root' must not be empty, nor end with a '/' (except if `root' == "/")
     *
     * Otherwise, the "path" xattr will not be correct
     */
    assert(strlen(root) > 0);
    assert(strcmp(root, "/") == 0 || root[strlen(root) - 1] != '/');

    if (entry == NULL) {
        paths[0] = strdup(root);
    } else {
        assert(strcmp(root, "/") == 0 || *entry == '/' || *entry == '\0');
        if (asprintf(&paths[0], "%s%s", root, entry) < 0)
            paths[0] = NULL;
    }

    if (paths[0] == NULL)
        return NULL;

    posix_iter = malloc(sizeof(*posix_iter));
    if (posix_iter == NULL) {
        save_errno = errno;
        free(paths[0]);
        errno = save_errno;
        return NULL;
    }

    posix_iter->iterator = POSIX_ITER;
    posix_iter->statx_sync_type = statx_sync_type;
    posix_iter->prefix_len = strcmp(root, "/") ? strlen(root) : 0;
    posix_iter->fts_handle =
        fts_open(paths, FTS_PHYSICAL | FTS_NOSTAT | FTS_XDEV, NULL);
    save_errno = errno;
    free(paths[0]);
    if (posix_iter->fts_handle == NULL) {
        free(posix_iter);
        errno = save_errno;
        return NULL;
    }

    return posix_iter;
}

/*----------------------------------------------------------------------------*
 |                               posix_backend                                |
 *----------------------------------------------------------------------------*/

struct posix_backend {
    struct rbh_backend backend;
    char *root;
    int statx_sync_type;
};

    /*--------------------------------------------------------------------*
     |                            get_option()                            |
     *--------------------------------------------------------------------*/

static int
posix_get_statx_sync_type(struct posix_backend *posix, void *data,
                      size_t *data_size)
{
    int statx_sync_type = posix->statx_sync_type;

    if (*data_size < sizeof(statx_sync_type)) {
        *data_size = sizeof(statx_sync_type);
        errno = EOVERFLOW;
        return -1;
    }
    memcpy(data, &statx_sync_type, sizeof(statx_sync_type));
    *data_size = sizeof(statx_sync_type);
    return 0;
}

static int
posix_backend_get_option(void *backend, unsigned int option, void *data,
                         size_t *data_size)
{
    struct posix_backend *posix = backend;

    switch (option) {
    case RBH_PBO_STATX_SYNC_TYPE:
        return posix_get_statx_sync_type(posix, data, data_size);
    }

    errno = ENOPROTOOPT;
    return -1;
}

    /*--------------------------------------------------------------------*
     |                            set_option()                            |
     *--------------------------------------------------------------------*/

static int
posix_set_statx_sync_type(struct posix_backend *posix, const void *data,
                          size_t data_size)
{
    int statx_sync_type;

    if (data_size != sizeof(statx_sync_type)) {
        errno = EINVAL;
        return -1;
    }
    memcpy(&statx_sync_type, data, sizeof(statx_sync_type));

    switch (statx_sync_type) {
    case AT_STATX_FORCE_SYNC:
#ifndef HAVE_STATX
        /* Without the statx() system call, there is no guarantee that metadata
         * is actually synced by the remote filesystem
         */
        errno = ENOTSUP;
        return -1;
#else
        /* Fall through */
#endif
    case AT_STATX_SYNC_AS_STAT:
    case AT_STATX_DONT_SYNC:
        posix->statx_sync_type &= ~AT_STATX_SYNC_TYPE;
        posix->statx_sync_type |= statx_sync_type;
        return 0;
    }

    errno = EINVAL;
    return -1;
}

static int
posix_backend_set_option(void *backend, unsigned int option, const void *data,
                         size_t data_size)
{
    struct posix_backend *posix = backend;

    switch (option) {
    case RBH_PBO_STATX_SYNC_TYPE:
        return posix_set_statx_sync_type(posix, data, data_size);
    }

    errno = ENOPROTOOPT;
    return -1;
}

    /*--------------------------------------------------------------------*
     |                               root()                               |
     *--------------------------------------------------------------------*/

static struct rbh_fsentry *
posix_root(void *backend, const struct rbh_filter_projection *projection)
{
    const struct rbh_filter_options options = {
        .projection = *projection,
    };
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *root;
    int save_errno;

    fsentries = rbh_backend_filter(backend, NULL, &options);
    if (fsentries == NULL)
        return NULL;

    root = rbh_mut_iter_next(fsentries);
    save_errno = errno;
    rbh_mut_iter_destroy(fsentries);
    errno = save_errno;

    return root;
}

    /*--------------------------------------------------------------------*
     |                              filter()                              |
     *--------------------------------------------------------------------*/

/* Modify the root's name and parent ID to match RobinHood's conventions */
static void
set_root_properties(FTSENT *root)
{
    /* The content of fts_pointer is only ever read, so casting away the
     * const modifier of `ROOT_PARENT_ID' is harmless.
     */
    root->fts_parent->fts_pointer = (void *)&ROOT_PARENT_ID;

    /* XXX: could this mess up fts' internal buffers?
     *
     * It does not seem to.
     */
    root->fts_name[0] = '\0';
    root->fts_namelen = 0;
}

static struct rbh_mut_iterator *
posix_backend_filter(void *backend, const struct rbh_filter *filter,
                     const struct rbh_filter_options *options)
{
    struct posix_backend *posix = backend;
    struct posix_iterator *posix_iter;
    struct rbh_fsentry *fsentry;
    int save_errno;

    /* TODO: make use of `fsentries_mask' and `statx_mask' */

    if (filter != NULL) {
        errno = ENOTSUP;
        return NULL;
    }

    if (options->skip > 0 || options->limit > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    posix_iter = posix_iterator_new(posix->root, NULL, posix->statx_sync_type);
    if (posix_iter == NULL)
        return NULL;

    fsentry = rbh_mut_iter_next(&posix_iter->iterator);
    if (fsentry == NULL)
        goto out_destroy_iter;
    free(fsentry);

    set_root_properties(posix_iter->ftsent);
    if (fts_set(posix_iter->fts_handle, posix_iter->ftsent, FTS_AGAIN))
        /* This should never happen */
        goto out_destroy_iter;

    return &posix_iter->iterator;

out_destroy_iter:
    save_errno = errno;
    rbh_mut_iter_destroy(&posix_iter->iterator);
    errno = save_errno;
    return NULL;
}

    /*--------------------------------------------------------------------*
     |                             destroy()                              |
     *--------------------------------------------------------------------*/

static void
posix_backend_destroy(void *backend)
{
    struct posix_backend *posix = backend;

    free(posix->root);
    free(posix);
}

    /*--------------------------------------------------------------------*
     |                              branch()                              |
     *--------------------------------------------------------------------*/

static int
open_by_id(const char *root, const struct rbh_id *id, int flags)
{
    struct file_handle *handle;
    int save_errno;
    int mount_fd;
    int fd = -1;

    handle = rbh_file_handle_from_id(id);
    if (handle == NULL)
        return -1;

    mount_fd = open(root, O_RDONLY | O_CLOEXEC);
    save_errno = errno;
    if (mount_fd < 0)
        goto out_free_handle;

    fd = open_by_handle_at(mount_fd, handle, flags);
    save_errno = errno;

    /* Ignore errors on close */
    close(mount_fd);

out_free_handle:
    free(handle);
    errno = save_errno;
    return fd;
}

static char *
fd2path(int fd)
{
    size_t pathlen = page_size - 1;
    char *path = NULL;
    int proc_fd;
    int save_errno;
    char *tmp;

    if (asprintf(&tmp, "/proc/self/fd/%d", fd) < 0) {
        errno = ENOMEM;
        return NULL;
    }

    proc_fd = open(tmp, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_PATH);
    save_errno = errno;
    free(tmp);
    if (proc_fd < 0)
        goto out;

    path = freadlink(proc_fd, &pathlen);
    save_errno = errno;

    /* Ignore errors on close */
    close(proc_fd);
out:
    errno = save_errno;
    return path;
}

static char *
id2path(const char *root, const struct rbh_id *id)
{
    char *path;
    int fd;
    int save_errno;

    fd = open_by_id(root, id, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_PATH);
    if (fd < 0)
        return NULL;

    path = fd2path(fd);

    /* Ignore errors on close */
    save_errno = errno;
    close(fd);
    errno = save_errno;
    return path;
}

struct posix_branch_backend {
    struct posix_backend posix;
    struct rbh_id id;
};

static struct rbh_mut_iterator *
posix_branch_backend_filter(void *backend, const struct rbh_filter *filter,
                            const struct rbh_filter_options *options)
{
    struct posix_branch_backend *branch = backend;
    struct posix_iterator *posix_iter;
    char *root, *path;
    int save_errno;

    if (filter != NULL) {
        errno = ENOTSUP;
        return NULL;
    }

    if (options->skip > 0 || options->limit > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    root = realpath(branch->posix.root, NULL);
    if (root == NULL)
        return NULL;

    path = id2path(root, &branch->id);
    save_errno = errno;
    if (path == NULL) {
        free(root);
        errno = save_errno;
        return NULL;
    }

    assert(strncmp(root, path, strlen(root)) == 0);
    posix_iter = posix_iterator_new(root, path + strlen(root),
                                    branch->posix.statx_sync_type);
    save_errno = errno;
    free(path);
    free(root);
    errno = save_errno;

    return (struct rbh_mut_iterator *)posix_iter;
}

static struct rbh_backend *
posix_backend_branch(void *backend, const struct rbh_id *id);

static const struct rbh_backend_operations POSIX_BRANCH_BACKEND_OPS = {
    .root = posix_root,
    .branch = posix_backend_branch,
    .filter = posix_branch_backend_filter,
    .destroy = posix_backend_destroy,
};

static const struct rbh_backend POSIX_BRANCH_BACKEND = {
    .name = RBH_POSIX_BACKEND_NAME,
    .ops = &POSIX_BRANCH_BACKEND_OPS,
};

static struct rbh_backend *
posix_backend_branch(void *backend, const struct rbh_id *id)
{
    struct posix_backend *posix = backend;
    struct posix_branch_backend *branch;
    size_t data_size;
    char *data;

    data_size = id->size;
    branch = malloc(sizeof(*branch) + data_size);
    if (branch == NULL)
        return NULL;
    data = (char *)branch + sizeof(*branch);

    branch->posix.root = strdup(posix->root);
    if (branch->posix.root == NULL) {
        int save_errno = errno;

        free(branch);
        errno = save_errno;
        return NULL;
    }

    branch->posix.statx_sync_type = posix->statx_sync_type;
    rbh_id_copy(&branch->id, id, &data, &data_size);
    branch->posix.backend = POSIX_BRANCH_BACKEND;

    return &branch->posix.backend;
}

static const struct rbh_backend_operations POSIX_BACKEND_OPS = {
    .get_option = posix_backend_get_option,
    .set_option = posix_backend_set_option,
    .branch = posix_backend_branch,
    .root = posix_root,
    .filter = posix_backend_filter,
    .destroy = posix_backend_destroy,
};

static const struct rbh_backend POSIX_BACKEND = {
    .id = RBH_BI_POSIX,
    .name = RBH_POSIX_BACKEND_NAME,
    .ops = &POSIX_BACKEND_OPS,
};

static size_t
rtrim(char *string, char c)
{
    size_t length = strlen(string);

    while (length > 0 && string[length - 1] == c)
        string[--length] = '\0';

    return length;
}

struct rbh_backend *
rbh_posix_backend_new(const char *path)
{
    struct posix_backend *posix;

    posix = malloc(sizeof(*posix));
    if (posix == NULL)
        return NULL;

    posix->root = strdup(*path == '\0' ? "." : path);
    if (posix->root == NULL) {
        int save_errno = errno;

        free(posix);
        errno = save_errno;
        return NULL;
    }

    if (rtrim(posix->root, '/') == 0)
        *posix->root = '/';

    posix->statx_sync_type = AT_STATX_SYNC_AS_STAT;
    posix->backend = POSIX_BACKEND;

    return &posix->backend;
}
