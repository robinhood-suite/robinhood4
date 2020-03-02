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

#include "utils.h"

struct rbh_fsentry *
rbh_fsentry_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                const char *name, const struct statx *statxbuf,
                const char *symlink)
{
    struct rbh_fsentry *fsentry;
    size_t symlink_length = 0;
    size_t name_length = 0;
    size_t size = 0;
    char *data;

    if (symlink) {
        if (statxbuf && (statxbuf->stx_mask & STATX_TYPE)
                && !S_ISLNK(statxbuf->stx_mode)) {
            errno = EINVAL;
            return NULL;
        }
        symlink_length = strlen(symlink) + 1;
        size += symlink_length;
    }
    if (id)
        size += id->size;
    if (parent_id)
        size += parent_id->size;
    if (name) {
        name_length = strlen(name) + 1;
        size += name_length;
    }
    if (statxbuf) {
        size = sizealign(size, alignof(*fsentry->statx));
        size += sizeof(*statxbuf);
    }

    fsentry = calloc(1, sizeof(*fsentry) + size);
    if (fsentry == NULL)
        return NULL;
    data = fsentry->symlink;

    /* fsentry->symlink (set first because it is a flexible array) */
    if (symlink) {
        assert(size >= symlink_length);
        data = mempcpy(data, symlink, symlink_length);
        size -= symlink_length;
        fsentry->mask |= RBH_FP_SYMLINK;
    }

    /* fsentry->id */
    if (id) {
        int rc = rbh_id_copy(&fsentry->id, id, &data, &size);
        assert(rc == 0);
        fsentry->mask |= RBH_FP_ID;
    }

    /* fsentry->parent_id */
    if (parent_id) {
        int rc = rbh_id_copy(&fsentry->parent_id, parent_id, &data, &size);
        assert(rc == 0);
        fsentry->mask |= RBH_FP_PARENT_ID;
    }

    /* fsentry->name */
    if (name) {
        assert(size >= name_length);
        fsentry->name = data;
        data = mempcpy(data, name, name_length);
        size -= name_length;
        fsentry->mask |= RBH_FP_NAME;
    }

    /* fsentry->statx */
    if (statxbuf) {
        data = ptralign(data, &size, alignof(*fsentry->statx));
        assert(size >= sizeof(*statxbuf));
        fsentry->statx = (struct statx *)data;
        data = mempcpy(data, statxbuf, sizeof(*statxbuf));
        size -= sizeof(*statxbuf);
        fsentry->mask |= RBH_FP_STATX;
    }

    /* scan-build: intentional dead store */
    (void)data;
    (void)size;

    return fsentry;
}
