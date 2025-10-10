/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include "robinhood/backend.h"
#include "robinhood/fsentry.h"
#include "robinhood/itertools.h"
#include "robinhood/statx.h"

#include "undelete.h"

static struct rbh_fsentry *
get_fsentry_from_metadata_source_with_path(struct rbh_backend *metadata_source,
                                           const char *path)
{
    const struct rbh_filter_projection ALL = {
        /**
         * Archived entries that have been deleted no longer have a PARENT_ID
         * or a NAME stored inside the database
         */
        .fsentry_mask = RBH_FP_ALL & ~RBH_FP_PARENT_ID & ~RBH_FP_NAME,
        .statx_mask = RBH_STATX_ALL,
    };
    const struct rbh_filter PATH_FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAMESPACE_XATTRS,
                .xattr = "path"
            },
            .value = {
                .type = RBH_VT_STRING,
                .string = path
            },
        },
    };
    struct rbh_fsentry *fsentry;

    fsentry = rbh_backend_filter_one(metadata_source, &PATH_FILTER, &ALL);
    if (fsentry == NULL) {
        errno = ENOENT;
        fprintf(stderr, "Failed to find '%s' in source URI: %s (%d)\n",
                path, strerror(errno), errno);
        return NULL;
    }

    if (!(fsentry->mask & RBH_FP_STATX)) {
        errno = EINVAL;
        fprintf(stderr,
                "Entry '%s' in source URI is missing statx information: %s (%d)",
                path, strerror(errno), errno);
        return NULL;
    }

    return fsentry;
}

int
undelete(struct undelete_context *context, const char *output)
{
    struct rbh_fsevent delete_event = { .type = RBH_FET_DELETE };
    struct rbh_iterator *delete_iter;
    struct rbh_fsentry *new_fsentry;
    struct rbh_fsentry *fsentry;
    int save_errno;
    int rc;

    fsentry = get_fsentry_from_metadata_source_with_path(
        context->source,
        context->relative_target_path
    );
    if (fsentry == NULL)
        return -1;

    delete_event.id = fsentry->id;

    new_fsentry = rbh_backend_undelete(context->target,
                                       output ? output :
                                                context->absolute_target_path,
                                       fsentry);
    if (new_fsentry == NULL) {
        fprintf(stderr, "Failed to undelete '%s'\n",
                context->absolute_target_path);
        return -1;
    }

    delete_iter = rbh_iter_array(&delete_event, sizeof(delete_event), 1, NULL);
    if (delete_iter == NULL) {
        fprintf(stderr, "Failed to create iter array: %s (%d)\n",
                strerror(errno), errno);
        return -1;
    }

    rc = rbh_backend_update(context->source, delete_iter);
    save_errno = errno;
    rbh_iter_destroy(delete_iter);
    errno = save_errno;
    if (rc < 0) {
        fprintf(stderr,
                "Failed to update source URI to remove obsolete entry: %s (%d)\n",
                strerror(errno), errno);
        return -1;
    }

    return 0;
}
