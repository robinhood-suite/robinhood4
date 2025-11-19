/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "rbh_update_path.h"
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

static const struct rbh_value *
get_parent_path(struct rbh_backend *backend, struct rbh_fsentry *entry,
                struct rbh_update_context *ctx)
{
    /* If we are updating the first batch (from the mirror), we need to get
     * the parent of each entry because we are not sure that all entries
     * have the same parent, unlike in all the next batches.
     */
    if (ctx->value_path == NULL ||
        (ctx->value_path != NULL && ctx->from_mirror)) {

        if(ctx->parent && ctx->from_mirror)
            free(ctx->parent);

        ctx->parent = get_entry_parent(backend, entry);
        if (ctx->parent == NULL) {
            /* Skip this entry if it doesn't have a parent, will be updated
             * later
             */
            if (errno == ENODATA)
                return NULL;

            if (errno == RBH_BACKEND_ERROR)
                error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
            else
                error(EXIT_FAILURE, errno,
                      "failed to get the parent of '%s'", entry->name);
        }

        ctx->value_path = rbh_fsentry_find_ns_xattr(ctx->parent, "path");
        /* Skip this entry if it's parent doesn't have a path, will be updated
         * later
         */
        if (ctx->value_path == NULL) {
            errno = ENODATA;
            return NULL;
        }
    }

    return ctx->value_path;
}

struct rbh_fsevent *
get_entry_path(struct rbh_backend *backend, struct rbh_fsentry *entry,
               struct rbh_update_context *ctx)
{
    const struct rbh_value *value_path;
    struct rbh_fsevent *fsevent;

    value_path = get_parent_path(backend, entry, ctx);
    if (value_path == NULL)
        return NULL;

    fsevent = generate_fsevent_update_path(entry, value_path);

    return fsevent;
}
