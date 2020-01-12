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
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *fsentry;
    int save_errno = errno;

    fsentries = rbh_backend_filter_fsentries(backend, &FILTER, fsentry_mask,
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

static const struct rbh_id ROOT_PARENT_ID = {
    .data = NULL,
    .size = 0,
};

struct rbh_fsentry *
rbh_backend_fsentry_from_path(struct rbh_backend *backend, char *path,
                              unsigned int fsentry_mask,
                              unsigned int statx_mask)
{
    const struct rbh_id *id = &ROOT_PARENT_ID;
    struct rbh_fsentry *fsentry = NULL;
    struct rbh_fsentry *save_fsentry;
    int save_errno;

    if (path[0] != '\0' && path[0] != '/') {
        errno = EINVAL;
        return NULL;
    }

    while (true) {
        char *slash;

        slash = strchr(path, '/');
        if (slash == NULL)
            break;
        *slash++ = '\0';

        /* Look for the next character that is not a '/' */
        while (*slash++ == '/');
        if (*slash == '\0')
            break;

        save_fsentry = fsentry;
        fsentry = fsentry_from_parent_and_name(backend, id, path, RBH_FP_ID, 0);
        save_errno = errno;
        free(save_fsentry);
        errno = save_errno;
        if (fsentry == NULL)
            return NULL;

        if (!(fsentry->mask & RBH_FP_ID)) {
            free(fsentry);
            errno = ENODATA;
            return NULL;
        }

        id = &fsentry->id;
        path = slash;
    }

    save_fsentry = fsentry;
    fsentry = fsentry_from_parent_and_name(backend, id, path, fsentry_mask,
                                           statx_mask);
    save_errno = errno;
    free(save_fsentry);
    errno = save_errno;
    return fsentry;
}
