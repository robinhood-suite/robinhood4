/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "utils.h"

#include <robinhood.h>

static struct rbh_fsentry *
get_entry_parent(struct rbh_backend *backend, struct rbh_fsentry *entry)
{
    const struct rbh_filter_projection proj = {
        .fsentry_mask = RBH_FP_ID | RBH_FP_PARENT_ID | RBH_FP_NAME |
                        RBH_FP_NAMESPACE_XATTRS,
        .statx_mask = 0,
    };
    const struct rbh_filter_field *field;
    struct rbh_fsentry *parent;
    struct rbh_filter *filter;

    field = str2filter_field("id");
    filter = rbh_filter_compare_binary_new(RBH_FOP_EQUAL, field,
                                           entry->parent_id.data,
                                           entry->parent_id.size);
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "failed to create filter");

    parent = rbh_backend_filter_one(backend, filter, &proj);

    free(filter);

    return parent;
}

void
update_entry_path(struct rbh_backend *backend, struct rbh_fsentry *entry)
{
    const struct rbh_value *value_path;
    struct rbh_iterator *update_iter;
    struct rbh_fsevent *fsevent;
    struct rbh_fsentry *parent;
    int rc;

    /* Update entry path */
    parent = get_entry_parent(backend, entry);
    if (entry == NULL) {
        /* Skip this entry if it doesn't have a parent, will be update
         * later
         */
        if (errno == ENODATA)
            return;

        if (errno == RBH_BACKEND_ERROR)
            error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
        else
            error(EXIT_FAILURE, errno,
                  "failed to get the parent of '%s'", entry->name);
    }

    value_path = rbh_fsentry_find_ns_xattr(parent, "path");
    /* Skip this entry if it's parent doesn't have a path, will be update
     * later
     */
    if (value_path == NULL) {
        errno = ENODATA;
        return;
    }

    fsevent = generate_fsevent_update_path(entry, parent, value_path);
    update_iter = rbh_iter_array(fsevent, sizeof(*fsevent), 1, NULL);

    rc = rbh_backend_update(backend, update_iter);
    if (rc == -1)
        error(EXIT_FAILURE, errno, "failed to update '%s'", entry->name);

    rbh_iter_destroy(update_iter);
    free(fsevent);
    free(parent);
}
