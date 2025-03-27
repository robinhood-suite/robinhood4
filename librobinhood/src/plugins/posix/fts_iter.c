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
#include <unistd.h>

#include <robinhood/backends/posix_extension.h>
#include <robinhood/value.h>

struct fts_iterator {
    struct posix_iterator posix;
    FTS *fts_handle;
    FTSENT *ftsent;
};

static __thread struct rbh_sstack *sstack;

__attribute__((destructor))
static void
free_sstack(void)
{
    if (sstack)
        rbh_sstack_destroy(sstack);
}

static struct rbh_fsentry *
fsentry_from_ftsent(FTSENT *ftsent, int statx_sync_type, size_t prefix_len,
                    const struct rbh_posix_extension **enrichers)
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
                                       statx_sync_type,
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

/* When discovering a directory, the current children_counter is incremented and
 * it is saved to the sstack. Then, its own counter is initialized to 0. For
 * each entry inside, the counter is incremented; if an error occurs, it is
 * decremented since the entry will not be synchronized. When exiting a
 * directory (post-order), the directory’s counter is set aside, and the
 * parent’s counter is retrieved from the sstack to continue processing. An
 * fsevent is then generated to update the current directory (the one being
 * exited) in the destination backend.
 */
static __thread int children_counter = 0;

static void *
fts_iter_next(void *iterator)
{
    struct fts_iterator *iter = iterator;
    bool skip_error = iter->posix.skip_error;
    struct rbh_fsentry *fsentry = NULL;
    int save_errno = errno;
    int current_counter;
    size_t readable;
    FTSENT *ftsent;

    if (sstack == NULL) {
        sstack = rbh_sstack_new(1 << 10);
        if (sstack == NULL)
            return NULL;
    }

skip:
    errno = 0;
    ftsent = fts_read(iter->fts_handle);
    if (ftsent == NULL) {
        errno = errno ? : ENODATA;
        return NULL;
    }
    errno = save_errno;
    iter->ftsent = ftsent;

    switch (ftsent->fts_info) {
    case FTS_D:
        /* Increment the counter and save it to be retrieved after exploring
         * this new directory
         */
        children_counter++;
        RBH_SSTACK_PUSH(sstack, &children_counter, sizeof(int));
        /* Set the counter to 0 as we are exploring a new directory */
        children_counter = 0;
        break;
    case FTS_F:
    case FTS_NSOK:
        children_counter++;
        break;
    case FTS_DP:
        /* Save the current counter in a temporary variable */
        current_counter = children_counter;

        /* Retrieve the counter of the parent directory as we will continue to
         * explore the parent directory and update 'counter_children' in the
         * next call of 'fts_iter_next'
         */
        children_counter = *(int *) rbh_sstack_peek(sstack, &readable);
        rbh_sstack_pop(sstack, sizeof(int));

        /* If the directory we are leaving has children, we generate a fsevent
         * to update the destination backend
         */
        if (current_counter > 0)
            fsentry = build_fsentry_nb_children(ftsent->fts_pointer,
                                                current_counter);

        /* fsentry_from_ftsent() memoizes ids of directories */
        free(ftsent->fts_pointer);

        if (fsentry == NULL && current_counter > 0) {
            if (skip_error) {
                fprintf(stderr,
                        "Update of number of children of '%s' skipped\n",
                        ftsent->fts_path);
                goto skip;
            }
            return NULL;
        }

        if (fsentry)
            return fsentry;
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
             /* If we can't read a directory, retrieve the parent's counter
              * as we will continue to explore the parent's directory.
              */
             if (ftsent->fts_info == FTS_DNR) {
                children_counter = *(int *) rbh_sstack_peek(sstack, &readable);
                rbh_sstack_pop(sstack, sizeof(int));
                children_counter--;
             }
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

        ftsent->fts_parent->fts_pointer = id_from_fd(fd, RBH_BI_POSIX);
        save_errno = errno;
        close(fd);
        free(path_dup);
        errno = save_errno;
        if (ftsent->fts_parent->fts_pointer == NULL)
            return NULL;
    }

    fsentry = fsentry_from_ftsent(ftsent, iter->posix.statx_sync_type,
                                  iter->posix.prefix_len,
                                  iter->posix.enrichers);

    if (fsentry == NULL && (errno == ENOENT || errno == ESTALE)) {
        /* The entry moved from under our feet */
        if (skip_error) {
            fprintf(stderr, "Synchronization of '%s' skipped\n",
                    ftsent->fts_path);
            children_counter--;
            goto skip;
        }

        return NULL;
    }

    return fsentry;
}

static void
fts_iter_destroy(void *iterator)
{
    struct fts_iterator *iter = iterator;
    FTSENT *ftsent;

    while ((ftsent = fts_read(iter->fts_handle)) != NULL) {
        switch (ftsent->fts_info) {
        case FTS_D:
            fts_set(iter->fts_handle, ftsent, FTS_SKIP);
            break;
        case FTS_DP:
            /* fsentry_from_ftsent() memoizes ids of directories */
            free(ftsent->fts_pointer);
            break;
        }
    }
    fts_close(iter->fts_handle);
    free(iter);
}

static const struct rbh_mut_iterator_operations FTS_ITER_OPS = {
    .next = fts_iter_next,
    .destroy = fts_iter_destroy,
};

static const struct rbh_mut_iterator FTS_ITER = {
    .ops = &FTS_ITER_OPS,
};

struct rbh_mut_iterator *
fts_iter_new(const char *root, const char *entry, int statx_sync_type)
{
    char *paths[2] = {NULL, NULL};
    struct fts_iterator *iter;
    int save_errno;
    int rc;

    iter = malloc(sizeof(*iter));
    if (!iter)
        return NULL;

    rc = posix_iterator_setup(&iter->posix, root, entry, statx_sync_type);
    save_errno = errno;
    if (rc == -1)
        goto free_iter;

    paths[0] = iter->posix.path;
    iter->fts_handle = fts_open(paths, FTS_PHYSICAL | FTS_NOSTAT | FTS_XDEV,
                                NULL);

    save_errno = errno;
    free(iter->posix.path);
    if (!iter->fts_handle)
        goto free_iter;

    iter->posix.iterator = FTS_ITER;

    return (struct rbh_mut_iterator *)iter;

free_iter:
    save_errno = errno;
    free(iter);
    errno = save_errno;

    return NULL;
}

static const struct rbh_id ROOT_PARENT_ID = {
    .data = NULL,
    .size = 0,
};

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

int
fts_iter_root_setup(struct posix_iterator *_iter)
{
    struct fts_iterator *iter = (struct fts_iterator *)_iter;
    struct rbh_fsentry *fsentry;

    fsentry = rbh_mut_iter_next(&_iter->iterator);
    if (fsentry == NULL)
        return -1;

    free(fsentry);

    set_root_properties(iter->ftsent);
    if (fts_set(iter->fts_handle, iter->ftsent, FTS_AGAIN))
        return -1;

    return 0;
}

bool
rbh_posix_iter_is_fts(struct posix_iterator *iter)
{
    return iter->iterator.ops == &FTS_ITER_OPS;
}
