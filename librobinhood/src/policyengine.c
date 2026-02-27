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
rbh_collect_fsentries(struct rbh_backend *backend, struct rbh_filter *filter)
{
    struct rbh_mut_iterator *it;

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

    return it;
}

int
rbh_pe_execute(struct rbh_mut_iterator *mirror_iter,
               struct rbh_backend *mirror_backend,
               const char *fs_uri,
               const struct rbh_policy *policy)
{
    struct rbh_fsentry *mirror_entry;
    struct rbh_backend *fs_backend;
    struct rbh_raw_uri *raw_uri;
    struct rbh_uri *uri;
    int save_errno;

    raw_uri = rbh_raw_uri_from_string(fs_uri);
    if (raw_uri == NULL)
        error(EXIT_FAILURE, errno, "rbh_raw_uri_from_string failed for '%s'",
              fs_uri);

    uri = rbh_uri_from_raw_uri(raw_uri);
    free(raw_uri);
    if (uri == NULL)
        error(EXIT_FAILURE, errno, "rbh_uri_from_raw_uri failed for '%s'",
              fs_uri);

    fs_backend = rbh_backend_and_branch_from_uri(uri, true);
    save_errno = errno;
    free(uri);

    if (!fs_backend)
        error(EXIT_FAILURE, save_errno,
              "rbh_backend_and_branch_from_uri failed for '%s'", fs_uri);

    while (true) {
        const struct rbh_rule *matched_rule = NULL;
        struct rbh_fsentry *fresh;

        errno = 0;
        mirror_entry = rbh_mut_iter_next(mirror_iter);

        if (mirror_entry == NULL) {
            if (errno == ENODATA)
                break;
            if (errno == EAGAIN)
                continue;

            fprintf(stderr, "Error during iteration: %s\n", strerror(errno));
            rbh_backend_destroy(fs_backend);
            return -1;
        }

        fresh = rbh_get_fresh_fsentry(fs_backend, mirror_entry);
        if (fresh == NULL) {
            fprintf(stderr, "Warning: cannot get fresh metadata %s\n",
                    strerror(errno));
            continue;
        }

        // First, check if entry matches the policy's default filter
        if (!rbh_filter_matches_fsentry(policy->filter, fresh)) {
            free(fresh);
            continue;
        }

        // Now try to match a rule
        for (size_t i = 0; i < policy->rule_count; i++) {
            const struct rbh_rule *rule = &policy->rules[i];
            if (rbh_filter_matches_fsentry(rule->filter, fresh)) {
                matched_rule = rule;
                printf("Rule: %s matched\n", matched_rule->name);
                break;
            }
        }

        free(fresh);
    }

    rbh_backend_destroy(fs_backend);
    rbh_backend_destroy(mirror_backend);
    rbh_mut_iter_destroy(mirror_iter);

    return 0;
}
