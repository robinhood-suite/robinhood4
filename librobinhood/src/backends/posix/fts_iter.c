/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <errno.h>
#include <fts.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>

#include <robinhood/backends/posix_extension.h>
#include <robinhood/backends/posix_internal.h>
#include <robinhood/value.h>
#include <robinhood/utils.h>

static struct rbh_fsentry *
fsentry_from_ftsent(FTSENT *ftsent, int statx_sync_type, size_t prefix_len,
                    inode_xattrs_callback_t inode_xattrs_callback,
                    enricher_t *enrichers)
{
    const struct rbh_value path = {
        .type = RBH_VT_STRING,
        .string = ftsent->fts_pathlen == prefix_len ?
            "/" : ftsent->fts_path + prefix_len,
    };
    struct fsentry_id_pair pair;
    bool fsentry_success;
    int save_errno;

    fsentry_success = fsentry_from_any(&pair, &path, ftsent->fts_accpath,
                                       ftsent->fts_pointer,
                                       ftsent->fts_parent->fts_pointer,
                                       ftsent->fts_name,
                                       statx_sync_type, inode_xattrs_callback,
                                       enrichers);
    save_errno = errno;

    if (!fsentry_success)
        return NULL;

    switch (ftsent->fts_info) {
    case FTS_D:
        /* memoize ids of directories */
        ftsent->fts_pointer = pair.id;
        break;
    default:
        free(pair.id);
    }

    errno = save_errno;
    return pair.fsentry;
}

static void *
fts_iter_next(void *iterator)
{
    struct posix_iterator *posix_iter = iterator;
    bool skip_error = posix_iter->skip_error;
    struct rbh_fsentry *fsentry;
    int save_errno = errno;
    short backend_id;
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
    case FTS_NS: /* May include ENAMETOOLONG errors */
        errno = ftsent->fts_errno;
        fprintf(stderr, "FTS: failed to read entry '%s': %s (%d)\n",
                ftsent->fts_path, strerror(errno), errno);
        if (skip_error) {
             fprintf(stderr, "Synchronization of '%s' skipped\n",
                     ftsent->fts_path);
             goto skip;
        }
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

        backend_id = (posix_iter->inode_xattrs_callback == NULL) ?
            RBH_BI_POSIX : RBH_BI_LUSTRE;
        ftsent->fts_parent->fts_pointer = id_from_fd(fd, backend_id);
        save_errno = errno;
        close(fd);
        free(path_dup);
        errno = save_errno;
        if (ftsent->fts_parent->fts_pointer == NULL)
            return NULL;
    }

    fsentry = fsentry_from_ftsent(ftsent, posix_iter->statx_sync_type,
                                  posix_iter->prefix_len,
                                  posix_iter->inode_xattrs_callback,
                                  posix_iter->enrichers);
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
fts_iter_destroy(void *iterator)
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

static const struct rbh_mut_iterator_operations FTS_ITER_OPS = {
    .next = fts_iter_next,
    .destroy = fts_iter_destroy,
};

const struct rbh_mut_iterator FTS_ITER = {
    .ops = &FTS_ITER_OPS,
};
