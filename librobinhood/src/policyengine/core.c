/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <errno.h>
#include <fnmatch.h>
#include <libgen.h>
#include <limits.h>
#include <regex.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sysexits.h>

#include "robinhood/policyengine/core.h"
#include "robinhood/policyengine/actions.h"
#include "robinhood/filters/core.h"
#include "robinhood/plugins/common_ops.h"
#include <robinhood.h>

static const char *
extract_string_info(struct rbh_backend *backend, int info_flag,
                    const char *key)
{
    struct rbh_value_map *info_map = rbh_backend_get_info(backend, info_flag);
    const struct rbh_value *val;

    if (!info_map)
        return NULL;

    val = rbh_map_find(info_map, key);
    if (!val)
        return NULL;

    if (val->type == RBH_VT_STRING)
        return val->string;

    if (val->type == RBH_VT_SEQUENCE) {
        for (size_t i = 0; i < val->sequence.count; i++) {
            const struct rbh_value_map *map = &val->sequence.values[i].map;
            const struct rbh_value *type = rbh_map_find(map, "type");
            const struct rbh_value *plugin = rbh_map_find(map, "plugin");

            if (type && strcmp(type->string, "plugin") == 0 && plugin)
                return plugin->string;
        }
    }

    return NULL;
}

const char *
rbh_backend_get_source_backend(struct rbh_backend *backend)
{
    return extract_string_info(backend, RBH_INFO_BACKEND_SOURCE,
                               "backend_source");
}

const char *
rbh_backend_get_mountpoint(struct rbh_backend *backend)
{
    return extract_string_info(backend, RBH_INFO_MOUNTPOINT,
                               "mountpoint");
}

static bool
rbh_pe_resolve_sort_field(struct rbh_backend *backend,
                          const char *sort_by,
                          struct rbh_filter_field *field)
{
    struct rbh_value_map *info_map = NULL;
    struct filters_context f_ctx = {0};

    if (!sort_by || !*sort_by || !field)
        return false;

    *field = (struct rbh_filter_field){0};

    info_map = rbh_backend_get_info(backend, RBH_INFO_BACKEND_SOURCE);
    if (!info_map)
        return false;

    import_plugins(&f_ctx, &info_map, 1);

    for (size_t i = 0; i < f_ctx.info_pe_count; ++i) {
        const struct rbh_pe_common_operations *ops;
        struct rbh_filter_field candidate;

        ops = get_common_operations(&f_ctx.info_pe[i]);
        candidate = rbh_pe_common_ops_sort2field(ops, sort_by);
        if (candidate.fsentry) {
            *field = candidate;
            filters_ctx_finish(&f_ctx);
            return true;
        }
    }

    filters_ctx_finish(&f_ctx);
    return false;
}

struct rbh_mut_iterator *
rbh_collect_fsentries(struct rbh_backend *backend,
                      struct rbh_filter *filter,
                      const struct rbh_policy_sort *sort)
{
    struct rbh_mut_iterator *it;
    struct rbh_filter_sort sort_item = {0};

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

    if (sort && sort->by && *sort->by) {
        struct rbh_filter_field field;
        const char *sort_by = sort->by;

        if (!rbh_pe_resolve_sort_field(backend, sort_by, &field)) {
            error(EXIT_FAILURE, EINVAL,
                  "unknown or unsupported sort field '%s'", sort_by);
            __builtin_unreachable();
        }

        sort_item.field = field;
        sort_item.ascending = sort->ascending;
        options.sort.items = &sort_item;
        options.sort.count = 1;

        output.projection.fsentry_mask |= field.fsentry;
        if (field.fsentry == RBH_FP_STATX)
            output.projection.statx_mask |= field.statx;
    }

    it = rbh_backend_filter(backend, filter, &options, &output, NULL);
    if (!it)
        error(EXIT_FAILURE, errno, "rbh_backend_filter failed");

    return it;
}

/**
 * Match a rule against a fresh fsentry.
 *
 * This function iterates over all rules in the policy and returns the first
 * rule whose filter matches the provided fsentry. A rule with a NULL filter
 * matches unconditionally.
 *
 * @param policy        the policy containing the rules to evaluate
 * @param fresh         the fresh fsentry to test against rule filters
 * @param matched_index output index of the matched rule when one is found
 *
 * @return              true if a rule matched, false otherwise
 */
static bool
rbh_pe_match_rule(const struct rbh_policy *policy,
                  const struct rbh_fsentry *fresh,
                  size_t *matched_index)
{
    for (size_t i = 0; i < policy->rule_count; i++) {
        const struct rbh_rule *rule = &policy->rules[i];
        if (rule->filter == NULL ||
            rbh_filter_matches_fsentry(rule->filter, fresh)) {
            *matched_index = i;
            return true;
        }
    }

    return false;
}

static size_t *
rbh_pe_action_used_counter(struct rbh_action_cache *cache,
                           bool has_matched_rule,
                           size_t matched_index)
{
    if (has_matched_rule) {
        if (matched_index >= cache->rule_count)
            return NULL;
        return &cache->rule_count_used[matched_index];
    }

    return &cache->default_count_used;
}

int
rbh_pe_execute(struct rbh_mut_iterator *mirror_iter,
               struct rbh_backend *mirror_backend,
               const char *fs_uri,
               const struct rbh_policy *policy)
{
    struct rbh_action_cache action_cache = {0};
    struct rbh_value_map *info_map = NULL;
    struct filters_context f_ctx = {0};
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

    rbh_pe_actions_init(policy, &action_cache);

    /* Load plugin/extension info into f_ctx */
    info_map = rbh_backend_get_info(fs_backend, RBH_INFO_BACKEND_SOURCE);
    if (info_map)
        import_plugins(&f_ctx, &info_map, 1);

    while (true) {
        struct rbh_action current_action;
        bool has_matched_rule = false;
        struct rbh_fsentry *fresh;
        size_t matched_index = 0;
        size_t limit;
        size_t *used;

        errno = 0;
        mirror_entry = rbh_mut_iter_next(mirror_iter);

        if (mirror_entry == NULL) {
            if (errno == ENODATA)
                break;
            if (errno == EAGAIN)
                continue;

            fprintf(stderr, "Error during iteration: %s\n",
                    rbh_strerror(errno));
            rbh_pe_actions_destroy(&action_cache);
            rbh_backend_destroy(fs_backend);
            return -1;
        }

        fresh = rbh_get_fresh_fsentry(fs_backend, mirror_entry);
        if (fresh == NULL) {
            fprintf(stderr, "Warning: cannot get fresh metadata %s\n",
                    rbh_strerror(errno));
            continue;
        }

        // First, check if entry matches the policy's default filter
        if (!rbh_filter_matches_fsentry(policy->filter, fresh)) {
            free(fresh);
            continue;
        }

        has_matched_rule = rbh_pe_match_rule(policy, fresh, &matched_index);
        current_action = rbh_pe_select_action(policy, &action_cache,
                                              has_matched_rule, matched_index);

        /* Count-based limiting */
        limit = rbh_pe_action_count_limit(&current_action);
        used = rbh_pe_action_used_counter(&action_cache, has_matched_rule,
                                          matched_index);

        if (limit > 0 && used && *used >= limit) {
            free(fresh);
            continue;
        }
        rbh_pe_apply_action(&current_action, fresh, mirror_backend, fs_backend,
                            &f_ctx);

        if (limit > 0 && used)
            (*used)++;

        free(fresh);
    }

    filters_ctx_finish(&f_ctx);
    rbh_pe_actions_destroy(&action_cache);
    rbh_backend_destroy(fs_backend);
    rbh_backend_destroy(mirror_backend);
    rbh_mut_iter_destroy(mirror_iter);

    return 0;
}
