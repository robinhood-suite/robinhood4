/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "robinhood/backend.h"

__thread char rbh_backend_error[512];

int
rbh_generic_backend_get_option(struct rbh_backend *backend, unsigned int option,
                               void *data, size_t *data_size)
{
    switch (option) {
    case RBH_GBO_DEPRECATED:
        errno = ENOTSUP;
        return -1;
    }

    errno = EINVAL;
    return -1;
}

int
rbh_generic_backend_set_option(struct rbh_backend *backend, unsigned int option,
                               const void *data, size_t data_size)
{
    switch (option) {
    case RBH_GBO_DEPRECATED:
        errno = ENOTSUP;
        return -1;
    }

    errno = EINVAL;
    return -1;
}

struct rbh_fsentry *
rbh_backend_filter_one(struct rbh_backend *backend,
                       const struct rbh_filter *filter,
                       unsigned int fsentry_mask, unsigned int statx_mask)
{
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *fsentry;
    int save_errno = errno;

    fsentries = rbh_backend_filter_fsentries(backend, filter, fsentry_mask,
                                             statx_mask);
    if (fsentries == NULL)
        return NULL;

    errno = 0;
    fsentry = rbh_mut_iter_next(fsentries);
    if (fsentry == NULL) {
        assert(errno);
        save_errno = errno == ENODATA ? ENOENT : errno;
    }

    rbh_mut_iter_destroy(fsentries);
    errno = save_errno;
    return fsentry;
}

static struct rbh_fsentry *
fsentry_from_parent_and_name(struct rbh_backend *backend,
                             const struct rbh_id *parent_id, const char *name,
                             unsigned int fsentry_mask, unsigned int statx_mask)
{
    const struct rbh_filter PARENT_FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = RBH_FF_PARENT_ID,
            .value = {
                .type = RBH_FVT_BINARY,
                .binary = {
                    .size = parent_id->size,
                    .data = parent_id->data,
                },
            },
        },
    };
    const struct rbh_filter NAME_FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = RBH_FF_NAME,
            .value = {
                .type = RBH_FVT_STRING,
                .string = name,
            },
        },
    };
    const struct rbh_filter * const FILTERS[] = {
        &PARENT_FILTER,
        &NAME_FILTER,
    };
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_AND,
        .logical = {
            .count = ARRAY_SIZE(FILTERS),
            .filters = FILTERS,
        },
    };

    return rbh_backend_filter_one(backend, &FILTER, fsentry_mask, statx_mask);
}

static const struct rbh_id ROOT_PARENT_ID = {
    .data = NULL,
    .size = 0,
};

static struct rbh_fsentry *
backend_fsentry_from_path(struct rbh_backend *backend, char *path,
                          unsigned int fsentry_mask, unsigned int statx_mask)
{
    struct rbh_fsentry *fsentry;
    struct rbh_fsentry *parent;
    int save_errno;
    char *slash;

    if (*path == '/') {
        parent = fsentry_from_parent_and_name(backend, &ROOT_PARENT_ID, "",
                                              RBH_FP_ID, 0);
        /* Discard every leading '/' */
        do {
            path++;
        } while (*path == '/');
    } else {
        parent = rbh_backend_root(backend, RBH_FP_ID, 0);
    }
    if (parent == NULL)
        return NULL;
    if (!(parent->mask & RBH_FP_ID)) {
        free(parent);
        errno = ENODATA;
        return NULL;
    }

    while ((slash = strchr(path, '/'))) {
        *slash++ = '\0';

        /* Look for the next character that is not a '/' */
        while (*slash == '/')
            slash++;
        if (*slash == '\0')
            break;

        fsentry = fsentry_from_parent_and_name(backend, &parent->id, path,
                                               RBH_FP_ID, 0);
        save_errno = errno;
        free(parent);
        errno = save_errno;
        if (fsentry == NULL)
            return NULL;
        if (!(fsentry->mask & RBH_FP_ID)) {
            free(fsentry);
            errno = ENODATA;
            return NULL;
        }

        parent = fsentry;
        path = slash;
    }

    fsentry = fsentry_from_parent_and_name(backend, &parent->id, path,
                                           fsentry_mask, statx_mask);
    save_errno = errno;
    free(parent);
    errno = save_errno;
    return fsentry;
}

struct rbh_fsentry *
rbh_backend_fsentry_from_path(struct rbh_backend *backend, const char *path_,
                              unsigned int fsentry_mask,
                              unsigned int statx_mask)
{
    struct rbh_fsentry *fsentry;
    int save_errno;
    char *path;

    path = strdup(path_);
    if (path == NULL)
        return NULL;

    fsentry = backend_fsentry_from_path(backend, path, fsentry_mask,
                                        statx_mask);
    save_errno = errno;
    free(path);
    errno = save_errno;
    return fsentry;
}
