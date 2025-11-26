/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <errno.h>
#include <fnmatch.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sysexits.h>

#include "robinhood/filters/core.h"
#include "robinhood/policyengine.h"
#include <robinhood.h>

struct rbh_mut_iterator *
rbh_collect_fsentry(const char *uri, struct rbh_filter *filter)
{
    struct rbh_mut_iterator *it;

    struct rbh_backend *backend = rbh_backend_from_uri(uri, true);
    if (!backend)
        error(EXIT_FAILURE, errno, "rbh_backend_from_uri failed");

    struct rbh_filter_options options = {
        .skip = 0,
        .limit = 0,
        .skip_error = true,
        .one = false,
        .verbose = false,
        .dry_run = false
    };

    struct rbh_filter_output output = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = RBH_FP_ID | RBH_FP_NAMESPACE_XATTRS,
            .statx_mask = 0,
        }
    };

    it = rbh_backend_filter(backend, filter, &options, &output, NULL);
    if (!it)
        error(EXIT_FAILURE, errno, "rbh_backend_filter failed");

    rbh_backend_destroy(backend);

    return it;
}

static const char *
rbh_pe_get_path(const struct rbh_fsentry *fsentry)
{
    const struct rbh_value *path_value;

    path_value = rbh_fsentry_find_ns_xattr(fsentry, "path");
    if (path_value == NULL || path_value->type != RBH_VT_STRING)
        return NULL;

    return path_value->string;
}
