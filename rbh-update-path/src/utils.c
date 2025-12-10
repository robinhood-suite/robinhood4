/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "utils.h"

#include <robinhood.h>

struct rbh_mut_iterator *
get_entries(struct rbh_backend *backend, struct rbh_filter *filter)
{
    const struct rbh_filter_projection proj = {
        .fsentry_mask = RBH_FP_ID | RBH_FP_PARENT_ID | RBH_FP_NAME |
                        RBH_FP_STATX,
        .statx_mask = RBH_STATX_TYPE,
    };
    const struct rbh_filter_options option = {0};
    const struct rbh_filter_output output = {
        .type = RBH_FOT_PROJECTION,
        .projection = proj,
    };
    struct rbh_mut_iterator *fsentries;

    fsentries = rbh_backend_filter(backend, filter, &option, &output, NULL);
    if (fsentries == NULL) {
        if (errno == RBH_BACKEND_ERROR)
            error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
        else
            error(EXIT_FAILURE, errno,
                  "failed to execute filter on backend '%s'", backend->name);
    }

    free(filter);

    return fsentries;
}

struct rbh_fsevent *
generate_fsevent_ns_xattrs(struct rbh_fsentry *entry, struct rbh_value *value)
{
    struct rbh_value_map xattrs;
    struct rbh_fsevent *fsevent;
    struct rbh_value_pair pair;

    pair.key = "path";
    pair.value = value;

    xattrs.pairs = &pair;
    xattrs.count = 1;

    fsevent = rbh_fsevent_ns_xattr_new(&entry->id, &xattrs, &entry->parent_id,
                                       entry->name);

    if (fsevent == NULL)
        error(EXIT_FAILURE, errno, "failed to generate fsevent");

    return fsevent;
}

struct rbh_fsevent *
generate_fsevent_update_path(struct rbh_fsentry *entry,
                             struct rbh_fsentry *parent,
                             const struct rbh_value *value_path)
{
    struct rbh_fsevent *fsevent;
    struct rbh_value value;
    char *format;
    char *path;

    if (strcmp(value_path->string, "/") == 0)
        format = "%s%s";
    else
        format = "%s/%s";

    if (asprintf(&path, format, value_path->string, entry->name) == -1)
        error(EXIT_FAILURE, errno, "failed to create the path");

    value.type = RBH_VT_STRING;
    value.string = path;

    fsevent = generate_fsevent_ns_xattrs(entry, &value);

    free(path);

    return fsevent;
}
