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
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sysexits.h>

#include "robinhood/policyengine.h"
#include "robinhood/filters/core.h"
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

/**
 * Parse an action string into a structured rbh_action.
 *
 * @param action_str  raw action string from the policy (e.g. "common:delete",
 *                    "cmd:rsync ...", "py:func").
 *
 * @return            a parsed rbh_action describing the action type and its
 *                    associated value. Unknown formats are returned as
 *                    RBH_ACTION_UNKNOWN.
 *
 * This function performs lightweight parsing and is typically used together
 * with an action cache to avoid repeated parsing.
 */
static struct rbh_action
rbh_pe_parse_action(const char *action_str)
{
    struct rbh_action act = {0};

    if (action_str == NULL) {
        act.type = RBH_ACTION_UNKNOWN;
        act.value = NULL;
        return act;
    }

    if (strncmp(action_str, "cmd:", 4) == 0) {
        act.type = RBH_ACTION_CMD;
        act.value = action_str + 4;
        return act;
    }

    if (strncmp(action_str, "py:", 3) == 0) {
        act.type = RBH_ACTION_PYTHON;
        act.value = action_str + 3;
        return act;
    }

    if (strncmp(action_str, "common:", sizeof "common:" - 1) == 0) {
        const char *sub = action_str + (sizeof "common:" - 1);
        if (strcmp(sub, "delete") == 0) {
            act.type = RBH_ACTION_DELETE;
            act.value = NULL;
            return act;
        } else if (strcmp(sub, "log") == 0) {
            act.type = RBH_ACTION_LOG;
            act.value = NULL;
            return act;
        }
    }

    act.type = RBH_ACTION_UNKNOWN;
    act.value = action_str;
    return act;
}

/**
 * Initialize the action cache for a policy.
 *
 * This function sets the default action to RBH_ACTION_UNSET and allocates
 * the per‑rule action array when the policy defines rules. All rule actions
 * are initialized to RBH_ACTION_UNSET.
 *
 * @param policy    the policy whose rules determine the cache size
 * @param cache     the action cache to initialize
 */
static void
rbh_pe_actions_init(const struct rbh_policy *policy,
                    struct rbh_action_cache *cache)
{
    cache->default_action = (struct rbh_action){0};
    cache->default_action.type = RBH_ACTION_UNSET;

    cache->rule_count = policy->rule_count;

    if (policy->rule_count == 0)
        return;

    cache->rule_actions = xcalloc(policy->rule_count,
                                  sizeof(*cache->rule_actions));

    for (size_t i = 0; i < policy->rule_count; i++)
        cache->rule_actions[i].type = RBH_ACTION_UNSET;
}

/**
 * Destroy the action cache.
 *
 * This function frees the per‑rule action array if it was allocated and
 * resets the cache fields to their default values.
 *
 * @param cache     the action cache to destroy
 */
static void
rbh_pe_actions_destroy(struct rbh_action_cache *cache)
{
    if (cache->default_action.params.initialized)
        rbh_sstack_destroy(cache->default_action.params.sstack);

    for (size_t i = 0; i < cache->rule_count; i++) {
        if (cache->rule_actions && cache->rule_actions[i].params.initialized)
            rbh_sstack_destroy(cache->rule_actions[i].params.sstack);
    }

    free(cache->rule_actions);
    cache->rule_actions = NULL;
    cache->rule_count = 0;
}

/**
 * Initialize the parameters of a parsed action.
 *
 * Allocates an sstack and converts the JSON parameter string into a
 * rbh_value_map. If parameters is NULL or conversion fails, the action
 * is left with params.initialized = false.
 *
 * @param parsed      the action whose params field will be filled
 * @param parameters  JSON parameter string, or NULL
 */
static void
rbh_pe_load_action_params(struct rbh_action *parsed, const char *parameters)
{
    parsed->params.sstack = NULL;
    parsed->params.initialized = false;

    if (!parameters)
        return;

    parsed->params.sstack = rbh_sstack_new(1 << 10);

    if (!rbh_action_parameters2value_map(parameters,
                                         &parsed->params.map,
                                         parsed->params.sstack)) {
        rbh_sstack_destroy(parsed->params.sstack);
        parsed->params.sstack = NULL;
    } else {
        parsed->params.initialized = true;
    }
}

/**
 * Select the action to apply for a matched entry.
 *
 * If the matched rule has an associated cached action, it is returned.
 * Otherwise, the action is parsed from the rule and stored in the cache.
 * If no rule matched, the policy's default action is parsed and cached
 * on first use.
 *
 * @param policy        the policy containing rule and default actions
 * @param cache         the action cache used to store parsed actions
 * @param has_rule      whether a rule matched the entry
 * @param matched_index index of the matched rule when has_rule is true
 *
 * @return              the selected action
 */
struct rbh_action
rbh_pe_select_action(const struct rbh_policy *policy,
                     struct rbh_action_cache *cache,
                     bool has_rule,
                     size_t matched_index)
{
    const char *parameters = NULL;
    struct rbh_action parsed;

    if (has_rule && cache->rule_actions != NULL) {
        if (cache->rule_actions[matched_index].type == RBH_ACTION_UNSET) {
            parsed = rbh_pe_parse_action(policy->rules[matched_index].action);
            parameters = policy->rules[matched_index].parameters;
            rbh_pe_load_action_params(&parsed, parameters);
            cache->rule_actions[matched_index] = parsed;
        }
        return cache->rule_actions[matched_index];
    }

    /* Default action */
    if (cache->default_action.type == RBH_ACTION_UNSET) {
        parsed = rbh_pe_parse_action(policy->action);
        parameters = policy->parameters;
        rbh_pe_load_action_params(&parsed, parameters);
        cache->default_action = parsed;
    }
    return cache->default_action;
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

/**
 * Apply an action to a filesystem entry.
 *
 * This function dispatches the action based on its type. The specific behavior
 * for each action type is implemented by the backend-specific apply_action
 * function.
 *
 * @param action        the action to apply (contains parsed parameters)
 * @param entry         the fsentry on which the action is applied
 * @param mi_backend    the mirror backend
 * @param fs_backend    the filesystem backend
 * @param common_ops    backend-specific common operations
 *
 * @return              0 on success, -1 on error
 */
static int
rbh_pe_apply_action(const struct rbh_action *action,
                    struct rbh_fsentry *entry,
                    struct rbh_backend *mi_backend,
                    struct rbh_backend *fs_backend,
                    const struct rbh_pe_common_operations *common_ops)
{
    switch (action->type) {
    case RBH_ACTION_LOG:
        return rbh_pe_common_ops_apply_action(common_ops, action, entry,
                                              mi_backend, fs_backend);
    case RBH_ACTION_DELETE:
        break;
    case RBH_ACTION_CMD:
        break;
    case RBH_ACTION_PYTHON:
        break;
    default:
        printf("Action type not supported\n");
        errno = ENOTSUP;
        return -1;
    }
    return 0;
}

int
rbh_pe_execute(struct rbh_mut_iterator *mirror_iter,
               struct rbh_backend *mirror_backend,
               const char *fs_uri,
               const struct rbh_policy *policy)
{
    const struct rbh_pe_common_operations *common_ops = NULL;
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

    /* Initialize common_ops once for all entries */
    info_map = rbh_backend_get_info(fs_backend, RBH_INFO_BACKEND_SOURCE);
    if (info_map) {
        import_plugins(&f_ctx, &info_map, 1);
        /* Find common_ops from plugin or extensions */
        for (size_t i = 0; i < f_ctx.info_pe_count; ++i) {
            const struct rbh_pe_common_operations *ops =
                get_common_operations(&f_ctx.info_pe[i]);
            if (ops) {
                common_ops = ops;
                break;
            }
        }
    }

    while (true) {
        struct rbh_action current_action;
        bool has_matched_rule = false;
        struct rbh_fsentry *fresh;
        size_t matched_index = 0;

        errno = 0;
        mirror_entry = rbh_mut_iter_next(mirror_iter);

        if (mirror_entry == NULL) {
            if (errno == ENODATA)
                break;
            if (errno == EAGAIN)
                continue;

            fprintf(stderr, "Error during iteration: %s\n", strerror(errno));
            rbh_pe_actions_destroy(&action_cache);
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

        has_matched_rule = rbh_pe_match_rule(policy, fresh, &matched_index);
        current_action = rbh_pe_select_action(policy, &action_cache,
                                              has_matched_rule, matched_index);

        rbh_pe_apply_action(&current_action, fresh, mirror_backend,fs_backend,
                            common_ops);

        free(fresh);
    }

    filters_ctx_finish(&f_ctx);
    rbh_pe_actions_destroy(&action_cache);
    rbh_backend_destroy(fs_backend);
    rbh_backend_destroy(mirror_backend);
    rbh_mut_iter_destroy(mirror_iter);

    return 0;
}
