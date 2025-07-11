/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "backend.h"
#include "value.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <error.h>

#include <robinhood/utils.h>

#include <sys/stat.h>
#include <sys/xattr.h>

#include "robinhood/backends/posix.h"
#include "robinhood/backend.h"
#include "posix_internals.h"
#include "robinhood/backends/posix_extension.h"
#include "robinhood/backends/posix.h"

#include "robinhood/plugins/backend.h"
#include "robinhood/sstack.h"
#include "robinhood/statx.h"
#include "robinhood/open.h"
#include <robinhood/utils.h>

#include "xattrs_mapping.h"

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

struct rbh_id *
id_from_fd(int fd, short backend_id)
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

        if (errno != EOVERFLOW || handle->handle_bytes <= handle_size) {
            if (errno == ENOTSUP)
                fprintf(stderr,
                        "'name_to_handle_at' call is not supported, cannot continue synchronization.\n");
            return NULL;
        }

        tmp = malloc(sizeof(*tmp) + handle->handle_bytes);
        if (tmp == NULL)
            return NULL;

        handle_size = handle->handle_bytes;
        free(handle);
        handle = tmp;
        goto retry;
    }

    return rbh_id_from_file_handle(handle, backend_id);
}

char *
freadlink(int fd, const char *path, size_t *size_)
{
    size_t size = *size_ + 1;
    char *symlink;
    ssize_t rc;

retry:
    symlink = malloc(size);
    if (symlink == NULL)
        return NULL;

    if (path == NULL)
        rc = readlinkat(fd, "", symlink, size);
    else
        rc = readlink(path, symlink, size);

    if (rc < 0) {
        int save_errno = errno;

        free(symlink);
        errno = save_errno;
        return NULL;
    }
    if ((size_t)rc == size) {
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
    return shift >= sizeof(number) * 8 ? number : 1u << shift;
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

            if ((size_t)length <= buflen)
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
    for (size_t i = 0; i < (size_t)count; i++, name += strlen(name) + 1) {
        struct rbh_value_pair *pair = &pairs[i - skipped];
        char buffer[XATTR_VALUE_MAX_VFS_SIZE];
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
        length = getxattr(proc_fd_path, name, buffer, sizeof(buffer));
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
        assert((size_t)length <= sizeof(buffer));
        buffer[length] = '\0';

        pair->value = create_value_from_xattr(name, buffer, length, xattrs);
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

void
build_pair_nb_children(struct rbh_value_pair *pair, int nb_children,
                       struct rbh_sstack *sstack)
{
    struct rbh_value *value;

    value = RBH_SSTACK_PUSH(sstack, NULL, sizeof(*pair->value));

    value->type = RBH_VT_INT64;
    value->int64 = nb_children;

    pair->key = "nb_children";
    pair->value = value;
}

struct rbh_fsentry *
build_fsentry_nb_children(struct rbh_id *id, int nb_children,
                          struct rbh_sstack *sstack)
{
    struct rbh_value_pair *pair;
    struct rbh_value_map xattr;

    if (sstack == NULL && xattrs != NULL)
        sstack = xattrs;

    pair = RBH_SSTACK_PUSH(sstack, NULL, sizeof(*pair));

    build_pair_nb_children(pair, nb_children, sstack);

    xattr.count = 1;
    xattr.pairs = pair;

    return rbh_fsentry_new(id, NULL, NULL, NULL, NULL, &xattr, NULL);
}

bool
fsentry_from_any(struct fsentry_id_pair *fip, const struct rbh_value *path,
                 char *accpath, struct rbh_id *entry_id,
                 struct rbh_id *parent_id, char *name, int statx_sync_type,
                 const struct rbh_posix_extension **enrichers)
{
    const int statx_flags =
        AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT;
    struct rbh_value_map inode_xattrs;
    struct rbh_value_map ns_xattrs;
    struct rbh_value_pair *pair;
    struct rbh_fsentry *fsentry;
    size_t pairs_count = 1 << 7;
    struct rbh_statx statxbuf;
    char proc_fd_path[64];
    char *symlink = NULL;
    struct rbh_id *id;
    ssize_t count = 0;
    int save_errno;
    int fd;

    if (pairs == NULL) {
        /* Per-thread initialization of `pairs' */
        pairs = reallocarray(NULL, pairs_count, sizeof(*pairs));
        if (pairs == NULL)
            return false;
    }

    if (ns_pairs == NULL) {
        /* Per-thread initialization of `ns_pairs' */
        ns_pairs = reallocarray(NULL, ns_pairs_count, sizeof(*ns_pairs));
        if (ns_pairs == NULL)
            return false;
    }

    if (values == NULL) {
        /* Per-thread initialization of `xattrs' */
        values = rbh_sstack_new(sizeof(ns_pairs->value) * pairs_count);
        if (values == NULL)
            return false;
    }

    if (xattrs == NULL) {
        /* Per-thread initialization of `values' */
        xattrs = rbh_sstack_new(XATTR_VALUE_MAX_VFS_SIZE);
        if (xattrs == NULL)
            return false;
    }

    if (ns_values == NULL) {
        /* Per-thread initialization of `ns_values' */
        ns_values = rbh_sstack_new(sizeof(ns_pairs->value) * ns_pairs_count);
        if (ns_values == NULL)
            return false;
    }

    fd = openat(AT_FDCWD, accpath,
                O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0 && (errno == ELOOP || errno == ENXIO))
        /* The open will fail with ENXIO if the entry is a socket, so open
         * it again but with O_PATH
         */
        fd = openat(AT_FDCWD, accpath,
                    O_PATH | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);

    if (fd < 0) {
        fprintf(stderr, "Failed to open '%s': %s (%d)\n",
                path->string, strerror(errno), errno);
        /* Set errno to ESTALE to not stop the iterator for a single failed
         * entry.
         */
        errno = ESTALE;
        return false;
    }

    if (sprintf(proc_fd_path, "/proc/self/fd/%d", fd) == -1) {
        errno = ENOMEM;
        return false;
    }

    /* The root entry might already have its ID computed and stored in
     * `entry_id'.
     */
    id = entry_id ? : id_from_fd(fd, RBH_BI_POSIX);
    if (id == NULL) {
        save_errno = errno;
        goto out_close;
    }

    if (rbh_statx(fd, "", statx_flags | statx_sync_type,
                  RBH_STATX_BASIC_STATS | RBH_STATX_BTIME | RBH_STATX_MNT_ID,
                  &statxbuf)) {
        fprintf(stderr, "Failed to stat '%s': %s (%d)\n",
                path->string, strerror(errno), errno);
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
        symlink = freadlink(fd, NULL, (size_t *)&statxbuf.stx_size);

        if (symlink == NULL) {
            fprintf(stderr, "Failed to readlink '%s': %s (%d)\n",
                    path->string, strerror(errno), errno);
            /* Set errno to ESTALE to not stop the iterator for a single failed
             * entry.
             */
            errno = ESTALE;
            save_errno = errno;
            goto out_free_id;
        }
    }

    if (S_ISLNK(statxbuf.stx_mode) || S_ISREG(statxbuf.stx_mode) ||
        S_ISDIR(statxbuf.stx_mode)) {
        count = getxattrs(proc_fd_path, &pairs, &pairs_count, values, xattrs);
        if (count == -1) {
            if (errno != ENOMEM) {
                fprintf(stderr, "Failed to get xattrs of '%s': %s (%d)\n",
                        path->string, strerror(errno), errno);
                /* Set errno to ESTALE to not stop the iterator for a single failed
                 * entry.
                 */
                errno = ESTALE;
            }
            save_errno = errno;
            goto out_clear_sstacks;
        }
    }

    pair = &ns_pairs[0];
    pair->key = "path";
    pair->value = RBH_SSTACK_PUSH(ns_values, path, sizeof(*path));

    ns_xattrs.count = 1;
    ns_xattrs.pairs = ns_pairs;

    if (enrichers != NULL) {
        struct entry_info info = {
            .fd = fd,
            .statx = &statxbuf,
            .inode_xattrs = pairs,
            .inode_xattrs_count = &count,
        };
        int callback_xattrs_count;
        int n_enricher = 0;

        while (enrichers[n_enricher]) {
            callback_xattrs_count = (*enrichers[n_enricher]->enrich)(
                &info, 0, &pairs[count],
                pairs_count - count, values
            );
            if (callback_xattrs_count == -1) {
                if (errno != ENOMEM) {
                    fprintf(stderr,
                            "Failed to get inode xattrs of '%s': %s (%d)\n",
                            path->string, strerror(errno), errno);
                    /* Set errno to ESTALE to not stop the iterator for a single
                     * failed entry.
                     */
                    errno = ESTALE;
                }
                save_errno = errno;
                goto out_clear_sstacks;
            }
            n_enricher++;
            count += callback_xattrs_count;
        }
    }

    if (S_ISDIR(statxbuf.stx_mode)) {
        build_pair_nb_children(&pairs[count], 0, xattrs);
        count++;
    }

    inode_xattrs.pairs = pairs;
    inode_xattrs.count = count;

    fsentry = rbh_fsentry_new(id, parent_id, name, &statxbuf,
                              &ns_xattrs, &inode_xattrs, symlink);

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

    fip->fsentry = fsentry;
    fip->id = id;

    return true;

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
    return false;
}

int
posix_iterator_setup(struct posix_iterator *iter,
                     const char *root,
                     const char *entry,
                     int statx_sync_type)
{
    /* `root' must not be empty, nor end with a '/' (except if `root' == "/")
     *
     * Otherwise, the "path" xattr will not be correct
     */
    assert(strlen(root) > 0);
    assert(strcmp(root, "/") == 0 || root[strlen(root) - 1] != '/');

    if (entry == NULL) {
        iter->path = strdup(root);
    } else {
        assert(strcmp(root, "/") == 0 || *entry == '/' || *entry == '\0');
        if (asprintf(&iter->path, "%s%s", root, entry) < 0)
            iter->path = NULL;
    }

    if (iter->path == NULL)
        return 0;

    iter->statx_sync_type = statx_sync_type;
    iter->prefix_len = strcmp(root, "/") ? strlen(root) : 0;

    return 0;
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
        .one = true,
    };
    const struct rbh_filter_output output = {
        .projection = *projection,
    };
    struct posix_backend *posix = backend;
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *root;
    iter_new_t old_iter_new;
    int save_errno;

    /* Since root only fetches one entry, no need to use a custom iterator for
     * this. This prevents the mfu iterator from walking the whole filesystem
     * just to fetch one entry.
     */
    old_iter_new = posix->iter_new;
    posix->iter_new = fts_iter_new;

    fsentries = rbh_backend_filter(backend, NULL, &options, &output);
    posix->iter_new = old_iter_new;
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

static struct rbh_mut_iterator *
posix_backend_filter(
    void *backend, const struct rbh_filter *filter,
    const struct rbh_filter_options *options,
    __attribute__((unused)) const struct rbh_filter_output *output)
{
    struct posix_backend *posix = backend;
    struct posix_iterator *posix_iter;
    char full_path[PATH_MAX];
    char root[PATH_MAX];
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

    if (options->one) {
        if (posix->root[0] == '/') {
            if (snprintf(root, 2, "/") == -1)
                error(EXIT_FAILURE, errno, "snprintf with '%s' and '/'", root);

            if (snprintf(full_path, PATH_MAX, "%s", posix->root) == -1)
                error(EXIT_FAILURE, errno, "snprintf with '%s' and '%s'",
                      full_path, posix->root);
        } else {
            if (getcwd(root, sizeof(root)) == NULL)
                error(EXIT_FAILURE, errno, "getcwd");

            if (snprintf(full_path, PATH_MAX, "%s/%s", root, posix->root) == -1)
                error(EXIT_FAILURE, errno, "snprintf with '%s', '%s' and '%s'",
                      full_path, root, posix->root);
        }
    }

    posix_iter = (struct posix_iterator *)
                  posix->iter_new(options->one ? root : posix->root,
                                  options->one ? full_path + strlen(root) : NULL,
                                  posix->statx_sync_type);
    posix_iter->enrichers = posix->enrichers;
    if (posix_iter == NULL)
        return NULL;

    posix_iter->skip_error = options->skip_error;

    if (options->one)
        /* Doesn't set the root's name to '\0' to keep the real root's name */
        return &posix_iter->iterator;

    /* FIXME move to iter_new? */
    if (rbh_posix_iter_is_fts(posix_iter) &&
        fts_iter_root_setup(posix_iter) == -1)
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

    free(posix->enrichers);
    free(posix->root);
    free(posix);
}

    /*--------------------------------------------------------------------*
     |                              branch()                              |
     *--------------------------------------------------------------------*/

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

    path = freadlink(proc_fd, NULL, &pathlen);
    save_errno = errno;

    /* Ignore errors on close */
    close(proc_fd);
out:
    errno = save_errno;
    return path;
}

char *
id2path(const char *root, const struct rbh_id *id)
{
    int save_errno;
    int mount_fd;
    char *path;
    int fd;

    mount_fd = mount_fd_by_root(root);
    if (mount_fd < 0)
        return NULL;

    fd = open_by_id_opath(mount_fd, id);
    if (fd < 0)
        return NULL;

    path = fd2path(fd);

    /* Ignore errors on close */
    save_errno = errno;
    close(fd);
    errno = save_errno;
    return path;
}

static struct rbh_mut_iterator *
posix_branch_backend_filter(
    void *backend, const struct rbh_filter *filter,
    const struct rbh_filter_options *options,
    __attribute__((unused)) const struct rbh_filter_output *output)
{
    struct posix_branch_backend *branch = backend;
    struct posix_iterator *posix_iter = NULL;
    char *root = NULL;
    char *path = NULL;
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
        if (path == NULL) {
            posix_iter = NULL;
            goto out;
        }
    }

    path = realpath(path, NULL);
    if (path == NULL) {
        posix_iter = NULL;
        goto out;
    }

    assert(strncmp(root, path, strlen(root)) == 0);
    posix_iter = (struct posix_iterator *)
                  branch->posix.iter_new(root, path + strlen(root),
                                         branch->posix.statx_sync_type);
    posix_iter->skip_error = options->skip_error;
    posix_iter->enrichers = branch->posix.enrichers;

out:
    save_errno = errno;
    free(path);
    free(root);
    errno = save_errno;

    return (struct rbh_mut_iterator *)posix_iter;
}

static struct rbh_backend *
posix_backend_branch(void *backend, const struct rbh_id *id, const char *path);

static struct rbh_value_map *
posix_get_info(void *backend, int info_flags);

static struct rbh_value_map *
posix_branch_get_info(void *backend, int info_flags)
{
    struct posix_branch_backend *branch = backend;

    return posix_get_info(&branch->posix, info_flags);
}

static const struct rbh_backend_operations POSIX_BRANCH_BACKEND_OPS = {
    .root = posix_root,
    .branch = posix_backend_branch,
    .filter = posix_branch_backend_filter,
    .get_info = posix_branch_get_info,
    .destroy = posix_backend_destroy,
};

static const struct rbh_backend POSIX_BRANCH_BACKEND = {
    .name = RBH_POSIX_BACKEND_NAME,
    .ops = &POSIX_BRANCH_BACKEND_OPS,
};

static int
enrichers_count(const struct rbh_posix_extension **enrichers)
{
    int count = 0;

    while (enrichers[count])
        count++;

    return count;
}

static const struct rbh_posix_extension **
dup_enrichers(const struct rbh_posix_extension **enrichers)
{
    const struct rbh_posix_extension **dup;
    int count;

    if (!enrichers)
        return NULL;

    count = enrichers_count(enrichers);
    dup = malloc(sizeof(*dup) * (count + 1)); // + 1 for NULL at the end
    if (!dup)
        return NULL;

    memcpy(dup, enrichers, sizeof(*enrichers) * (count + 1));

    return dup;
}

static struct rbh_backend *
posix_backend_branch(void *backend, const struct rbh_id *id, const char *path)
{
    struct posix_backend *posix = backend;
    struct posix_branch_backend *branch;
    size_t data_size;
    int save_errno;
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
        save_errno = errno;
        goto free_branch;
    }

    if (path) {
        branch->path = strdup(path);
        if (branch->path == NULL) {
            save_errno = errno;
            goto free_root;
        }
    } else {
        branch->path = NULL;
    }

    if (id)
        rbh_id_copy(&branch->id, id, &data, &data_size);
    else
        branch->id.size = 0;

    branch->posix.backend = POSIX_BRANCH_BACKEND;
    branch->posix.iter_new = posix->iter_new;
    errno = 0;
    branch->posix.enrichers = dup_enrichers(posix->enrichers);
    if (!branch->posix.enrichers && errno != 0) {
        save_errno = errno;
        goto free_path;
    }

    branch->posix.statx_sync_type = posix->statx_sync_type;

    return &branch->posix.backend;

free_path:
    free(branch->path);
free_root:
    free(branch->posix.root);
free_branch:
    free(branch);
    errno = save_errno;
    return NULL;
}

static int
posix_get_attribute(void *backend, uint64_t flags,
                    void *arg, struct rbh_value_pair *pairs,
                    int available_pairs)

{
    struct posix_backend *posix = backend;
    const struct rbh_posix_extension **enrichers = posix->enrichers;
    struct rbh_posix_enrich_ctx *ctx = arg;
    int n_enricher = 0;
    size_t count = 0;

    while (enrichers[n_enricher]) {
        size_t subcount;

        subcount = (*enrichers[n_enricher]->enrich)(&ctx->einfo, flags,
                                                    &pairs[count],
                                                    available_pairs - count,
                                                    ctx->values);
        if (subcount == -1)
            return -1;
        count += subcount;
        n_enricher++;
    }

    return count;
}

#define MIN_VALUES_SSTACK_ALLOC (1 << 6)
struct rbh_sstack *info_sstack;

static void __attribute__((destructor))
destroy_info_sstack(void)
{
    if (info_sstack)
        rbh_sstack_destroy(info_sstack);
}

/**
 * Output map will look like this for plugins:
 * `{"type": "plugin", "plugin": "posix"}`
 *
 * And like this for extensions:
 * `{"type": "extension", "plugin": "posix", "extension": "<extension_name>"}`
 */
struct rbh_value_map
rbh_posix_get_source_map(bool is_plugin, const char *extension_name,
                         struct rbh_sstack *sstack)
{
    int count = (is_plugin ? 2 : 3);
    struct rbh_value_map full_map;
    struct rbh_value_pair *pairs;
    struct rbh_value *values;

    pairs = RBH_SSTACK_PUSH(sstack, NULL, count * sizeof(*pairs));
    values = RBH_SSTACK_PUSH(sstack, NULL, count * sizeof(*values));

    pairs[0].key = "type";
    values[0].type = RBH_VT_STRING;
    values[0].string = (is_plugin ? "plugin" : "extension");
    pairs[0].value = &values[0];

    pairs[1].key = "plugin";
    values[1].type = RBH_VT_STRING;
    values[1].string = "posix";
    pairs[1].value = &values[1];

    if (!is_plugin) {
        pairs[2].key = "extension";
        values[2].type = RBH_VT_STRING;
        values[2].string = extension_name;
        pairs[2].value = &values[2];
    }

    full_map.pairs = pairs;
    full_map.count = count;

    return full_map;
}

/**
 * Output pair will look like this:
 * `"backend_source": [{<plugin or extension>}, {<plugin or extension>}, ...]`
 */
static int
get_source_backend(const struct posix_backend *posix,
                   struct rbh_value_pair *pair)
{
    struct rbh_value *backends_source_sequence;
    struct rbh_value *backends;
    int count = 0;
    int i;

    // Count all registered enrichers
    while (posix->enrichers && posix->enrichers[count] != NULL)
        count++;

    count += 1; // And add the POSIX plugin itself

    backends = RBH_SSTACK_PUSH(info_sstack, NULL, count * sizeof(*backends));
    for (i = 0; i < count - 1; ++i) {
        backends[i].type = RBH_VT_MAP;
        backends[i].map = rbh_posix_get_source_map(
            false,
            posix->enrichers[i]->extension.name,
            info_sstack
        );
    }

    backends[i].type = RBH_VT_MAP;
    backends[i].map = rbh_posix_get_source_map(true, NULL, info_sstack);

    backends_source_sequence = RBH_SSTACK_PUSH(
        info_sstack,
        NULL,
        sizeof(*backends_source_sequence)
    );
    backends_source_sequence->type = RBH_VT_SEQUENCE;
    backends_source_sequence->sequence.values = backends;
    backends_source_sequence->sequence.count = count;

    pair->key = "backend_source";
    pair->value = backends_source_sequence;

    return 0;
}

static struct rbh_value_map *
posix_get_info(void *backend, int info_flags)
{
    struct posix_backend *posix = backend;
    struct rbh_value_map *map_value;
    struct rbh_value_pair *pairs;
    int tmp_flags = info_flags;
    int count = 0;
    int idx = 0;

    while (tmp_flags) {
        count += tmp_flags & 1;
        tmp_flags >>= 1;
    }

    if (info_sstack == NULL) {
        info_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                     (sizeof(struct rbh_value_map *)));
        if (!info_sstack)
            goto out;
    }

    pairs = RBH_SSTACK_PUSH(info_sstack, NULL, count * sizeof(*pairs));
    map_value = RBH_SSTACK_PUSH(info_sstack, NULL, sizeof(*map_value));

    if (info_flags & RBH_INFO_BACKEND_SOURCE)
        if (get_source_backend(posix, &pairs[idx++]))
            goto out;

    map_value->pairs = pairs;
    map_value->count = idx;

    return map_value;
out:
    errno = EINVAL;
    return NULL;
}

static const struct rbh_backend_operations POSIX_BACKEND_OPS = {
    .get_option = posix_backend_get_option,
    .set_option = posix_backend_set_option,
    .branch = posix_backend_branch,
    .root = posix_root,
    .filter = posix_backend_filter,
    .get_attribute = posix_get_attribute,
    .get_info = posix_get_info,
    .destroy = posix_backend_destroy,
};

static const struct rbh_backend POSIX_BACKEND = {
    .id = RBH_BI_POSIX,
    .name = RBH_POSIX_BACKEND_NAME,
    .ops = &POSIX_BACKEND_OPS,
};

void
rbh_posix_helper(const char *type, struct rbh_config *config,
                 char **predicate_helper, char **directive_helper)
{
    struct posix_backend *posix = NULL;
    char ext_predicate_helper[8192];
    char ext_directive_helper[8192];
    size_t predicate_offset;
    size_t directive_offset;
    int count = 0;
    int rc;

    posix = malloc(sizeof(*posix));
    if (posix == NULL)
        goto err;

    posix->enrichers = NULL;

    if (type) {
        const struct rbh_plugin plugin = {
            .name = RBH_POSIX_BACKEND_NAME,
            .version = RBH_POSIX_BACKEND_VERSION,
        };

        if (load_posix_extensions(&plugin, posix, type, config) == -1)
            goto err;
    }

    ext_predicate_helper[0] = '\0';
    ext_directive_helper[0] = '\0';
    predicate_offset = 0;
    directive_offset = 0;

    while (posix->enrichers && posix->enrichers[count]) {
        char *ext_predicate = NULL;
        char *ext_directive = NULL;

        rbh_pe_common_ops_helper(posix->enrichers[count]->extension.common_ops,
                                 NULL, NULL, &ext_predicate, &ext_directive);

        if (ext_predicate) {
            size_t len = strlen(ext_predicate);

            memcpy(&ext_predicate_helper[predicate_offset], ext_predicate, len);
            ext_predicate_helper[len] = '\n';
            predicate_offset += len + 1;
        }

        if (ext_directive) {
            size_t len = strlen(ext_directive);

            memcpy(&ext_directive_helper[directive_offset], ext_directive, len);
            ext_directive_helper[len] = '\n';
            directive_offset += len + 1;
        }

        count++;
        free(ext_predicate);
        free(ext_directive);
    }

    rc = asprintf(predicate_helper,
        "  - POSIX: *Are listed only the differences between GNU's find and\n"
        "            rbh-find's POSIX predicates*:\n"
        "    -[acm]min [+-]TIME   filter entries based on their access,\n"
        "                         change or modify time. TIME should represent\n"
        "                         minutes, and the filtering will follow GNU's\n"
        "                         find logic for '-[acm]time'\n"
        "    -blocks [+-]N        filter entries based on their number of blocks\n"
        "    -size [+-]SIZE       filter entries based of their size. Works like\n"
        "                         GNU find's '-size' predicate except with the\n"
        "                         addition of the 'T' modifier for terabytes\n"
        "    -perm PERMISSIONS    filter entries based on their permissions,\n"
        "                         the '+' prefix is not supported\n"
        "\n"
        "%s", ext_predicate_helper);

    if (rc == -1)
        goto err;

    if (ext_directive_helper[0] != '\0') {
        if (asprintf(directive_helper, "%s", ext_directive_helper) == -1) {
            free(*predicate_helper);
            goto err;
        }
    } else {
        *directive_helper = NULL;
    }

    free(posix);

    return;

err:
    *predicate_helper = NULL;
    *directive_helper = NULL;
    free(posix);
}

static size_t
rtrim(char *string, char c)
{
    size_t length = strlen(string);

    while (length > 0 && string[length - 1] == c)
        string[--length] = '\0';

    return length;
}

struct rbh_backend *
rbh_posix_backend_new(const struct rbh_backend_plugin *self,
                      const struct rbh_uri *uri,
                      struct rbh_config *config,
                      bool read_only)
{
    const char *type = uri->backend;
    const char *path = uri->fsname;
    struct posix_backend *posix;
    int save_errno = 0;

    posix = malloc(sizeof(*posix));
    if (posix == NULL)
        return NULL;

    posix->root = strdup(*path == '\0' ? "." : path);
    if (posix->root == NULL) {
        save_errno = errno;
        goto free_backend;
    }

    if (rtrim(posix->root, '/') == 0)
        *posix->root = '/';

    posix->statx_sync_type = AT_RBH_STATX_SYNC_AS_STAT;
    posix->backend = POSIX_BACKEND;
    posix->enrichers = NULL;
    /* Default to FTS iterator */
    posix->iter_new = fts_iter_new;

    rbh_config_load(config);

    if (type) {
        int count = 0;

        if (load_posix_extensions(&self->plugin, posix, type, config) == -1) {
            save_errno = errno;
            goto free_root;
        }

        while (posix->enrichers && posix->enrichers[count]) {
            if (posix->enrichers[count]->setup_enricher)
                posix->enrichers[count]->setup_enricher();

            count++;
        }
    }

    if (set_xattrs_types_map()) {
        save_errno = errno;
        goto free_root;
    }

    return &posix->backend;

free_root:
    free(posix->root);
free_backend:
    free(posix);
    errno = save_errno;

    return NULL;
}
