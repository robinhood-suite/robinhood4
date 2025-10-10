/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <errno.h>
#include <error.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "robinhood/backend.h"
#include "robinhood/filter.h"
#include "robinhood/utils.h"
#include "robinhood/value.h"

#include "undelete.h"

static int
_list_deleted_entries(struct rbh_backend *metadata_source,
                      const char *path_regex)
{
    const struct rbh_filter_options OPTIONS = { 0 };
    const struct rbh_filter_output OUTPUT = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = RBH_FP_NAMESPACE_XATTRS,
            .statx_mask = 0,
        },
    };
    const struct rbh_filter RM_TIME_FILTER = {
        .op = RBH_FOP_EXISTS,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAMESPACE_XATTRS,
                .xattr = "rm_time"
            },
            .value = {
                .type = RBH_VT_BOOLEAN,
                .boolean = true
            },
        },
    };
    const struct rbh_filter PATH_PREFIX_FILTER = {
        .op = RBH_FOP_REGEX,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAMESPACE_XATTRS,
                .xattr = "path"
            },
            .value = {
                .type = RBH_VT_REGEX,
                .regex = {
                    .string = path_regex,
                    .options = 0
                },
            },
        },
    };
    const struct rbh_filter *AND_FILTERS[] = {
        &RM_TIME_FILTER,
        &PATH_PREFIX_FILTER
    };
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_AND,
        .logical = {
            .filters = AND_FILTERS,
            .count = sizeof(AND_FILTERS) / sizeof(*AND_FILTERS)
        }
    };
    struct rbh_mut_iterator *_fsentries;
    struct rbh_fsentry *fsentry;

    _fsentries = rbh_backend_filter(metadata_source, &FILTER, &OPTIONS,
                                    &OUTPUT, NULL);
    if (!_fsentries) {
        fprintf(stderr, "Failed to get undeletable entries in '%s': %s (%d)\n",
                path_regex, strerror(errno), errno);
        return -1;
    }

    printf("DELETED FILES:\n");
    while ((fsentry = rbh_mut_iter_next(_fsentries)) != NULL) {
        const struct rbh_value *ns_rm_time;
        const struct rbh_value *ns_rm_path;
        const char *rm_path;
        time_t rm_time;

        ns_rm_path = rbh_fsentry_find_ns_xattr(fsentry, "path");
        if (ns_rm_path == NULL) {
            fprintf(stderr, "'%s' is archived and deleted but has no rm_path\n",
                    fsentry->name);
            continue;
        }

        rm_path = ns_rm_path->string;

        ns_rm_time = rbh_fsentry_find_ns_xattr(fsentry, "rm_time");
        if (ns_rm_time == NULL) {
            fprintf(stderr, "'%s' is archived and deleted but has no rm_time\n",
                    fsentry->name);
            continue;
        }

        rm_time = (time_t) ns_rm_time->int64;

        printf("-- rm_path: %s   rm_time: %s \n", rm_path,
               time_from_timestamp(&rm_time));
    }

    rbh_mut_iter_destroy(_fsentries);

    return 0;
}

int
list_deleted_entries(struct undelete_context *context)
{
    char regex[PATH_MAX];

    if (snprintf(regex, sizeof(regex), "^%s",
                 context->relative_target_path) == -1) {
        fprintf(stderr,
                "Error while formatting regex associated with '%s': %s (%d)\n",
                context->relative_target_path, strerror(errno), errno);
        return -1;
    }

    return _list_deleted_entries(context->source, regex);
}
