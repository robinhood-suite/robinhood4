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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "robinhood/backend.h"

#include "utils.h"

__thread char rbh_backend_error[512];

/*----------------------------------------------------------------------------*
 |                      rbh_generic_backend_get_option()                      |
 *----------------------------------------------------------------------------*/

int
rbh_generic_backend_get_option(struct rbh_backend *backend, unsigned int option,
                               void *data, size_t *data_size)
{
    switch (option) {
    case RBH_GBO_DEPRECATED:
        errno = ENOTSUP;
        return -1;
    case RBH_GBO_GC:
        if (backend->ops->get_option == NULL) {
            errno = ENOTSUP;
            return -1;
        }
        return backend->ops->get_option(backend, option, data, data_size);
    }

    errno = EINVAL;
    return -1;
}

/*----------------------------------------------------------------------------*
 |                      rbh_generic_backend_set_option()                      |
 *----------------------------------------------------------------------------*/

int
rbh_generic_backend_set_option(struct rbh_backend *backend, unsigned int option,
                               const void *data, size_t data_size)
{
    switch (option) {
    case RBH_GBO_DEPRECATED:
        errno = ENOTSUP;
        return -1;
    case RBH_GBO_GC:
        if (backend->ops->set_option == NULL) {
            errno = ENOTSUP;
            return -1;
        }
        return backend->ops->set_option(backend, option, data, data_size);
    }

    errno = EINVAL;
    return -1;
}

/*----------------------------------------------------------------------------*
 |                          rbh_generic_filter_one()                          |
 *----------------------------------------------------------------------------*/

struct rbh_fsentry *
rbh_backend_filter_one(struct rbh_backend *backend,
                       const struct rbh_filter *filter,
                       const struct rbh_filter_projection *projection)
{
    const struct rbh_filter_options options = { 0 };
    const struct rbh_filter_output output = {
        .projection = *projection,
    };
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *fsentry;
    int save_errno = errno;

    fsentries = rbh_backend_filter(backend, filter, &options, &output);
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

/*----------------------------------------------------------------------------*
 |                      rbh_backend_fsentry_from_path()                       |
 *----------------------------------------------------------------------------*/

static struct rbh_fsentry *
fsentry_from_parent_and_name(struct rbh_backend *backend,
                             const struct rbh_id *parent_id, const char *name,
                             const struct rbh_filter_projection *projection)
{
    const struct rbh_filter PARENT_FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_PARENT_ID,
            },
            .value = {
                .type = RBH_VT_BINARY,
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
            .field = {
                .fsentry = RBH_FP_NAME,
            },
            .value = {
                .type = RBH_VT_STRING,
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

    return rbh_backend_filter_one(backend, &FILTER, projection);
}

static const struct rbh_id ROOT_PARENT_ID = {
    .data = NULL,
    .size = 0,
};

static struct rbh_fsentry *
backend_fsentry_from_path(struct rbh_backend *backend, char *path,
                          const struct rbh_filter_projection *projection)
{
    const struct rbh_filter_projection ID_ONLY = {
        .fsentry_mask = RBH_FP_ID,
    };
    struct rbh_fsentry *fsentry;
    struct rbh_fsentry *parent;
    int save_errno;
    char *slash;

    if (*path == '/') {
        /* Discard every leading '/' */
        do {
            path++;
        } while (*path == '/');

        if (*path == '\0')
            return fsentry_from_parent_and_name(backend, &ROOT_PARENT_ID, "",
                                                projection);

        parent = fsentry_from_parent_and_name(backend, &ROOT_PARENT_ID, "",
                                              &ID_ONLY);
    } else if (*path == '\0') {
        return rbh_backend_root(backend, projection);
    } else {
        parent = rbh_backend_root(backend, &ID_ONLY);
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
                                               &ID_ONLY);
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
                                           projection);
    save_errno = errno;
    free(parent);
    errno = save_errno;
    return fsentry;
}

struct rbh_fsentry *
rbh_backend_fsentry_from_path(struct rbh_backend *backend, const char *path_,
                              const struct rbh_filter_projection *projection)
{
    struct rbh_fsentry *fsentry;
    int save_errno;
    char *path;

    path = strdup(path_);
    if (path == NULL)
        return NULL;

    fsentry = backend_fsentry_from_path(backend, path, projection);
    save_errno = errno;
    free(path);
    errno = save_errno;
    return fsentry;
}

void
rbh_backend_error_printf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsnprintf(rbh_backend_error, sizeof(rbh_backend_error), fmt, args);
    va_end(args);

    errno = RBH_BACKEND_ERROR;
}
