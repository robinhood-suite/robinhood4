/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "utils.h"

#include <robinhood.h>

static struct rbh_mut_iterator *
get_entry_children(struct rbh_backend *backend, struct rbh_fsentry *entry)
{
    const struct rbh_filter_field *field;
    struct rbh_filter *filter;

    field = str2filter_field("parent-id");
    filter = rbh_filter_compare_binary_new(RBH_FOP_EQUAL, field,
                                           entry->id.data, entry->id.size);
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "failed to create filter");

    return get_entries(backend, filter);
}

bool
remove_children_path(struct rbh_backend *backend, struct rbh_fsentry *entry)
{
    struct rbh_mut_iterator *children;
    struct rbh_iterator *update_iter;
    struct rbh_fsevent *fsevent;
    bool need_update = false;
    int rc;

    children = get_entry_children(backend, entry);

    while (true) {
        struct rbh_fsentry *child = rbh_mut_iter_next(children);

        if (child == NULL) {
            if (errno == ENODATA)
                break;

            if (errno == RBH_BACKEND_ERROR)
                error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
            else
                error(EXIT_FAILURE, errno, "failed to retrieve child");
        }

        /* TODO: store all the fsevent in a list and call rbh_backend_update
         * one time
         */
        fsevent = generate_fsevent_ns_xattrs(child, NULL);
        update_iter = rbh_iter_array(fsevent,sizeof(*fsevent), 1, NULL);

        rc = rbh_backend_update(backend, update_iter);
        if (rc == -1)
            error(EXIT_FAILURE, errno, "failed to update '%s'", child->name);

        if (!need_update)
            need_update = true;
        rbh_iter_destroy(update_iter);
        free(fsevent);
        free(child);
    }

    return need_update;
}

