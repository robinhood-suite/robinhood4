/* This file is part of RobinHood 4
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
#include <sys/xattr.h>

#include "robinhood/backends/posix.h"
#include "robinhood/backends/posix_internal.h"
#include "robinhood/sstack.h"
#include "robinhood/statx.h"


/*----------------------------------------------------------------------------*
 |                               posix_iterator                               |
 *----------------------------------------------------------------------------*/

static __thread size_t handle_size = MAX_HANDLE_SZ;
static __thread struct file_handle *handle;

__attribute__((destructor))
void
free_handle(void)
{
    free(handle);
}

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

static size_t __attribute__((pure))
ceil2(size_t number)
{
    uint8_t shift;

    if (__builtin_popcountll(number) <= 1)
        return number;

    shift = sizeof(number) * 8 - __builtin_clzll(number);
    return shift >= sizeof(number) * 8 ? number : 1 << shift;
}

static ssize_t
flistxattrs(char *proc_fd_path, char **buffer, size_t *size)
{
    size_t buflen = *size;
    char *keys = *buffer;
    size_t count = 0;
    ssize_t length;

retry:
    length = listxattr(proc_fd_path, keys, buflen);
    if (length == -1) {
        void *tmp;

        switch (errno) {
        case E2BIG:
        case ENOTSUP:
            /* Not much we can do */
            return 0;
        case ERANGE:
            length = listxattr(proc_fd_path, NULL, 0);
            if (length == -1) {
                switch (errno) {
                case E2BIG:
                    /* Not much we can do */
                    return 0;
                default:
                    return -1;
                }
            }

            if (length <= buflen)
                /* The list of xattrs must have shrunk in between both calls */
                goto retry;

            tmp = realloc(keys, ceil2(length));
            if (tmp == NULL)
                return -1;
            *buffer = keys = tmp;
            *size = buflen = ceil2(length);

            goto retry;
        default:
            return -1;
        }
    }

    for (const char *key = keys; key < keys + length; key += strlen(key) + 1)
         count++;

    return count;
}

/* The Linux VFS does not allow values of more than 64KiB */
static const size_t XATTR_VALUE_MAX_VFS_SIZE = 1 << 16;

static __thread size_t names_length = 1 << 12;
static __thread char *names;

__attribute__((destructor))
void
free_names(void)
{
    free(names);
}

static ssize_t
getxattrs(char *proc_fd_path, struct rbh_value_pair **_pairs,
          size_t *_pairs_count,
          struct rbh_sstack *values, struct rbh_sstack *xattrs)
{
    struct rbh_value_pair *pairs = *_pairs;
    size_t pairs_count = *_pairs_count;
    size_t skipped = 0;
    ssize_t count;
    char *name;

    if (names == NULL) {
        names = malloc(names_length);
        if (names == NULL)
            return -1;
    }

    count = flistxattrs(proc_fd_path, &names, &names_length);
    if (count == -1)
        return -1;

    name = names;
    for (size_t i = 0; i < count; i++, name += strlen(name) + 1) {
        struct rbh_value_pair *pair = &pairs[i - skipped];
        char buffer[XATTR_VALUE_MAX_VFS_SIZE];
        struct rbh_value value = {
            .type = RBH_VT_BINARY,
        };
        ssize_t length;

        if (i - skipped == pairs_count) {
            void *tmp;

            tmp = reallocarray(pairs, pairs_count * 2, sizeof(*pairs));
            if (tmp == NULL)
                return -1;
            *_pairs = pairs = tmp;
            *_pairs_count = pairs_count *= 2;
        }
        assert(i - skipped < pairs_count);

        pair->key = name;
        length = getxattr(proc_fd_path, name, &buffer, sizeof(buffer));
        if (length == -1) {
            switch (errno) {
            case E2BIG:
            case ENODATA:
                skipped++;
                continue;
            default:
                /* The Linux VFS does not allow values of more than 64KiB */
                assert(errno != ERANGE);
                /* We should not be able to reach this point if the filesystem
                 * does not support extended attributes.
                 */
                assert(errno != ENOTSUP);
                return -1;
            }
        }
        assert(length <= sizeof(buffer));

        value.binary.data = rbh_sstack_push(xattrs, buffer, length);
        if (value.binary.data == NULL)
            return -1;
        value.binary.size = length;

        pair->value = rbh_sstack_push(values, &value, sizeof(value));
        if (pair->value == NULL)
            return -1;
    }

    return count - skipped;
}

static void
sstack_clear(struct rbh_sstack *sstack)
{
    size_t readable;

    while (true) {
        int rc;

        rbh_sstack_peek(sstack, &readable);
        if (readable == 0)
            break;

        rc = rbh_sstack_pop(sstack, readable);
        assert(rc == 0);
    }
}

static __thread struct rbh_value_pair *ns_pairs;
static __thread size_t ns_pairs_count = 1 << 7;
static __thread struct rbh_sstack *ns_values;
static __thread struct rbh_sstack *values;
static __thread struct rbh_sstack *xattrs;
static __thread struct rbh_value_pair *pairs;

__attribute__((destructor))
void
free_ns_data(void)
{
    free(ns_pairs);
    free(pairs);

    if (ns_values)
        rbh_sstack_destroy(ns_values);
    if (values)
        rbh_sstack_destroy(values);
    if (xattrs)
        rbh_sstack_destroy(xattrs);
}

static struct rbh_fsentry *
fsentry_from_ftsent(FTSENT *ftsent, int statx_sync_type, size_t prefix_len,
                    int (*inode_xattrs_callback)(const int,
                                                 const struct rbh_statx *,
                                                 struct rbh_value_pair *,
                                                 ssize_t *,
                                                 struct rbh_value_pair *,
                                                 struct rbh_sstack *))
{
    const int statx_flags =
        AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT;
    const struct rbh_value path = {
        .type = RBH_VT_STRING,
        .string = ftsent->fts_pathlen == prefix_len ?
            "/" : ftsent->fts_path + prefix_len,
    };
    struct rbh_value_map inode_xattrs;
    struct rbh_value_map ns_xattrs;
    struct rbh_value_pair *pair;
    struct rbh_fsentry *fsentry;
    size_t pairs_count = 1 << 7;
    struct rbh_statx statxbuf;
    char proc_fd_path[64];
    char *symlink = NULL;
    struct rbh_id *id;
    int save_errno;
    ssize_t count;
    int fd;

    if (pairs == NULL) {
        /* Per-thread initialization of `pairs' */
        pairs = reallocarray(NULL, pairs_count, sizeof(*pairs));
        if (pairs == NULL)
            return NULL;
    }

    if (ns_pairs == NULL) {
        /* Per-thread initialization of `ns_pairs' */
        ns_pairs = reallocarray(NULL, ns_pairs_count, sizeof(*ns_pairs));
        if (ns_pairs == NULL)
            return NULL;
    }

    if (values == NULL) {
        /* Per-thread initialization of `xattrs' */
        values = rbh_sstack_new(sizeof(ns_pairs->value) * pairs_count);
        if (values == NULL)
            return NULL;
    }

    if (xattrs == NULL) {
        /* Per-thread initialization of `values' */
        xattrs = rbh_sstack_new(XATTR_VALUE_MAX_VFS_SIZE);
        if (xattrs == NULL)
            return NULL;
    }

    if (ns_values == NULL) {
        /* Per-thread initialization of `ns_values' */
        ns_values = rbh_sstack_new(sizeof(ns_pairs->value) * ns_pairs_count);
        if (ns_values == NULL)
            return NULL;
    }

    fd = openat(AT_FDCWD, ftsent->fts_accpath,
                O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0 && (errno == ELOOP || errno == ENXIO))
        /* The open will fail with ENXIO if the entry is a socket, so open
         * it again but with O_PATH
         */
        fd = openat(AT_FDCWD, ftsent->fts_accpath,
                    O_PATH | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);

    if (fd < 0) {
        fprintf(stderr, "Failed to open '%s': %s (%d)\n",
                path.string, strerror(errno), errno);
        /* Set errno to ESTALE to not stop the iterator for a single failed
         * entry.
         */
        errno = ESTALE;
        return NULL;
    }

    if (sprintf(proc_fd_path, "/proc/self/fd/%d", fd) == -1) {
        errno = ENOMEM;
        return NULL;
    }

    /* The root entry might already have its ID computed and stored in
     * `fts_pointer'.
     */
    id = ftsent->fts_pointer ? : id_from_fd(fd);
    if (id == NULL) {
        save_errno = errno;
        goto out_close;
    }

    if (rbh_statx(fd, "", statx_flags | statx_sync_type,
                  RBH_STATX_BASIC_STATS | RBH_STATX_BTIME | RBH_STATX_MNT_ID,
                  &statxbuf)) {
        fprintf(stderr, "Failed to stat '%s': %s (%d)\n",
                path.string, strerror(errno), errno);
        /* Set errno to ESTALE to not stop the iterator for a single failed
         * entry.
         */
        errno = ESTALE;
        save_errno = errno;
        goto out_free_id;
    }

    /* We want the actual type of the file we opened, not the one fts saw */
    if (statxbuf.stx_mask & RBH_STATX_TYPE && S_ISLNK(statxbuf.stx_mode)) {
        if ((statxbuf.stx_mask & RBH_STATX_SIZE) == 0) {
            statxbuf.stx_size = page_size - 1;
            statxbuf.stx_mask |= RBH_STATX_SIZE;
        }
        static_assert(sizeof(size_t) == sizeof(statxbuf.stx_size), "");
        symlink = freadlink(fd, (size_t *)&statxbuf.stx_size);

        if (symlink == NULL) {
            fprintf(stderr, "Failed to readlink '%s': %s (%d)\n",
                    path.string, strerror(errno), errno);
            /* Set errno to ESTALE to not stop the iterator for a single failed
             * entry.
             */
            errno = ESTALE;
            save_errno = errno;
            goto out_free_id;
        }
    }

    count = getxattrs(proc_fd_path, &pairs, &pairs_count, values, xattrs);
    if (count == -1) {
        if (errno != ENOMEM) {
            fprintf(stderr, "Failed to get xattrs of '%s': %s (%d)\n",
                    path.string, strerror(errno), errno);
            /* Set errno to ESTALE to not stop the iterator for a single failed
            * entry.
            */
            errno = ESTALE;
        }
        save_errno = errno;
        goto out_clear_sstacks;
    }

    pair = &ns_pairs[0];
    pair->key = "path";
    pair->value = rbh_sstack_push(ns_values, &path, sizeof(path));
    if (pair->value == NULL) {
        save_errno = errno;
        goto out_clear_sstacks;
    }

    ns_xattrs.count = 1;
    ns_xattrs.pairs = ns_pairs;

    if (inode_xattrs_callback != NULL) {
        int callback_xattrs_count = inode_xattrs_callback(fd, &statxbuf, pairs,
                                                          &count, &pairs[count],
                                                          values);
        if (callback_xattrs_count == -1) {
            if (errno != ENOMEM) {
                fprintf(stderr,
                        "Failed to get inode xattrs of '%s': %s (%d)\n",
                        path.string, strerror(errno), errno);
                /* Set errno to ESTALE to not stop the iterator for a single
                 * failed entry.
                 */
                errno = ESTALE;
            }
            save_errno = errno;
            goto out_clear_sstacks;
        }

        count += callback_xattrs_count;
    }

    inode_xattrs.pairs = pairs;
    inode_xattrs.count = count;

    fsentry = rbh_fsentry_new(id, ftsent->fts_parent->fts_pointer,
                              ftsent->fts_name, &statxbuf, &ns_xattrs,
                              &inode_xattrs, symlink);
    if (fsentry == NULL) {
        save_errno = errno;
        goto out_clear_sstacks;
    }

    sstack_clear(values);
    sstack_clear(xattrs);
    sstack_clear(ns_values);
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

out_clear_sstacks:
    sstack_clear(values);
    sstack_clear(xattrs);
    sstack_clear(ns_values);

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
    bool skip_error = posix_iter->skip_error;
    struct rbh_fsentry *fsentry;
    int save_errno = errno;
    FTSENT *ftsent;

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
        errno = ftsent->fts_errno;
        fprintf(stderr, "FTS: failed to read entry '%s': %s (%d)\n",
                ftsent->fts_path, strerror(errno), errno);
        if (skip_error) {
             fprintf(stderr, "Synchronization of '%s' skipped\n",
                     ftsent->fts_path);
             goto skip;
        }
        return NULL;
    case FTS_NS:
        errno = ftsent->fts_errno;
        return NULL;
    }

    /* This condition checks if the entry has no parent, which indicates whether
     * the current ftsent is the root of our iterator or not, and if the first
     * character of its path is a '/', which indicates whether we are in a
     * branch or not. In that case, we open the parent of the branch point, and
     * set it so that the branch point has a proper path set in the database.
     */
    if (ftsent->fts_parent->fts_pointer == NULL &&
        ftsent->fts_accpath[0] == '/') {
        char *path_dup = strdup(ftsent->fts_accpath);
        char *first_slash;
        char *last_slash;
        int fd;

        if (path_dup == NULL)
            return NULL;

        last_slash = strrchr(path_dup, '/');
        if (last_slash == NULL) {
            save_errno = errno;
            free(path_dup);
            errno = save_errno;
            return NULL;
        }

        first_slash = strchr(path_dup, '/');
        if (first_slash != last_slash)
            last_slash[0] = 0;

        fd = openat(AT_FDCWD, path_dup, O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            save_errno = errno;
            free(path_dup);
            errno = save_errno;
            return NULL;
        }

        ftsent->fts_parent->fts_pointer = id_from_fd(fd);
        save_errno = errno;
        close(fd);
        free(path_dup);
        errno = save_errno;
        if (ftsent->fts_parent->fts_pointer == NULL)
            return NULL;
    }

    fsentry = fsentry_from_ftsent(ftsent, posix_iter->statx_sync_type,
                                  posix_iter->prefix_len,
                                  posix_iter->inode_xattrs_callback);
    if (fsentry == NULL && (errno == ENOENT || errno == ESTALE)) {
        /* The entry moved from under our feet */
        if (skip_error) {
            fprintf(stderr, "Synchronization of '%s' skipped\n",
                    ftsent->fts_path);
            goto skip;
        }
        return NULL;
    }

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

struct posix_iterator *
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
    posix_iter->inode_xattrs_callback = NULL;
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

int
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
    case AT_RBH_STATX_FORCE_SYNC:
#ifndef HAVE_STATX
        /* Without the statx() system call, there is no guarantee that metadata
         * is actually synced by the remote filesystem
         */
        errno = ENOTSUP;
        return -1;
#else
        /* Fall through */
#endif
    case AT_RBH_STATX_SYNC_AS_STAT:
    case AT_RBH_STATX_DONT_SYNC:
        posix->statx_sync_type &= ~AT_RBH_STATX_SYNC_TYPE;
        posix->statx_sync_type |= statx_sync_type;
        return 0;
    }

    errno = EINVAL;
    return -1;
}

int
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

struct rbh_fsentry *
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

struct rbh_mut_iterator *
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

    if (options->skip > 0 || options->limit > 0 || options->sort.count > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    posix_iter = posix->iter_new(posix->root, NULL, posix->statx_sync_type);
    if (posix_iter == NULL)
        return NULL;
    posix_iter->skip_error = options->skip_error;
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

void
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
    char *path;
};

void
posix_branch_backend_destroy(void *backend)
{
    struct posix_branch_backend *branch = backend;

    posix_backend_destroy(&branch->posix);
    free(branch);
}

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
    posix_iter = branch->posix.iter_new(root, path + strlen(root),
                                        branch->posix.statx_sync_type);
    save_errno = errno;
    free(path);
    free(root);
    errno = save_errno;

    return (struct rbh_mut_iterator *)posix_iter;
}

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

struct rbh_backend *
posix_backend_branch(void *backend, const struct rbh_id *id, const char *path)
{
    struct posix_backend *posix = backend;
    struct posix_branch_backend *branch;
    size_t data_size;
    char *data;

    if (id == NULL && path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    if (id) {
        data_size = id->size;
        branch = malloc(sizeof(*branch) + data_size);
        if (branch == NULL)
            return NULL;

        data = (char *)branch + sizeof(*branch);
    } else {
        branch = malloc(sizeof(*branch));
        if (branch == NULL)
            return NULL;
    }

    branch->posix.root = strdup(posix->root);
    if (branch->posix.root == NULL) {
        int save_errno = errno;

        free(branch);
        errno = save_errno;
        return NULL;
    }

    if (path) {
        branch->path = strdup(path);
        if (branch->path == NULL) {
            int save_errno = errno;

            free(branch->posix.root);
            free(branch);
            errno = save_errno;
            return NULL;
        }
    } else {
        branch->path = NULL;
    }

    if (id)
        rbh_id_copy(&branch->id, id, &data, &data_size);
    else
        branch->id.size = 0;

    branch->posix.iter_new = posix_iterator_new;
    branch->posix.statx_sync_type = posix->statx_sync_type;
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

    posix->iter_new = posix_iterator_new;
    posix->statx_sync_type = AT_RBH_STATX_SYNC_AS_STAT;
    posix->backend = POSIX_BACKEND;

    return &posix->backend;
}
