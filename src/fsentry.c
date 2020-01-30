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
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "robinhood/fsentry.h"
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

struct rbh_fsentry *
rbh_fsentry_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                const char *name, const struct statx *statxbuf,
                const char *symlink)
{
    struct rbh_fsentry *fsentry;
    size_t symlink_length = 0;
    size_t name_length = 0;
    size_t data_size = 0;
    char *data;

    if (name)
        name_length = strlen(name) + 1;
    if (symlink) {
        if (statxbuf && (statxbuf->stx_mask & STATX_TYPE)
                && !S_ISLNK(statxbuf->stx_mode)) {
            errno = EINVAL;
            return NULL;
        }
        symlink_length = strlen(symlink) + 1;
    }

    data_size += name_length + symlink_length;
    data_size += id ? id->size : 0;
    data_size += parent_id ? parent_id->size : 0;
    data_size += statxbuf ? sizeof(*statxbuf) : 0;

    fsentry = calloc(1, sizeof(*fsentry) + data_size);
    if (fsentry == NULL)
        return NULL;
    data = fsentry->symlink;

    /* fsentry->symlink (set first because it is a flexible array) */
    if (symlink) {
        data = mempcpy(data, symlink, symlink_length);
        fsentry->mask |= RBH_FP_SYMLINK;
    }

    /* fsentry->id */
    if (id) {
        fsentry->id.data = data;
        data = mempcpy(data, id->data, id->size);
        fsentry->id.size = id->size;
        fsentry->mask |= RBH_FP_ID;
    };

    /* fsentry->parent_id */
    if (parent_id) {
        fsentry->parent_id.data = data;
        data = mempcpy(data, parent_id->data, parent_id->size);
        fsentry->parent_id.size = parent_id->size;
        fsentry->mask |= RBH_FP_PARENT_ID;
    }

    /* fsentry->name */
    if (name) {
        fsentry->name = data;
        data = mempcpy(data, name, name_length);
        fsentry->mask |= RBH_FP_NAME;
    }

    /* fsentry->statx */
    if (statxbuf) {
        fsentry->statx = (struct statx *)data;
        data = mempcpy(data, statxbuf, sizeof(*statxbuf));
        fsentry->mask |= RBH_FP_STATX;
    }

    /* scan-build: intentional dead store */
    (void)data;

    return fsentry;
}
