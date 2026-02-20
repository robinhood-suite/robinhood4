/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <sysexits.h>

#include "robinhood/policyengine/actions.h"
#include "robinhood/action.h"
#include "robinhood/fsentry.h"
#include "robinhood/plugins/common_ops.h"
#include <robinhood.h>

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
 * the per-rule action array when the policy defines rules. All rule actions
 * are initialized to RBH_ACTION_UNSET.
 *
 * @param policy    the policy whose rules determine the cache size
 * @param cache     the action cache to initialize
 */
void
rbh_pe_actions_init(const struct rbh_policy *policy,
                    struct rbh_action_cache *cache)
{
    cache->default_action = (struct rbh_action){0};
    cache->default_action.type = RBH_ACTION_UNSET;

    cache->rule_count = policy->rule_count;

    if (policy->rule_count == 0)
        return;

    cache->rule_actions = calloc(policy->rule_count,
                                 sizeof(*cache->rule_actions));
    if (cache->rule_actions == NULL)
        error(EXIT_FAILURE, errno, "calloc failed");

    for (size_t i = 0; i < policy->rule_count; i++)
        cache->rule_actions[i].type = RBH_ACTION_UNSET;
}

/**
 * Destroy the action cache.
 *
 * This function frees the per-rule action array if it was allocated and
 * resets the cache fields to their default values.
 *
 * @param cache     the action cache to destroy
 */
void
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

            parsed.params.sstack = NULL;
            parsed.params.initialized = false;

            if (parameters) {
                parsed.params.sstack = rbh_sstack_new(1 << 10);
                if (!parsed.params.sstack)
                    error(EXIT_FAILURE, errno, "rbh_sstack_new failed");

                if (!rbh_action_parameters2value_map(parameters,
                                                     &parsed.params.map,
                                                     parsed.params.sstack)) {
                    rbh_sstack_destroy(parsed.params.sstack);
                    parsed.params.sstack = NULL;
                } else
                    parsed.params.initialized = true;
            }
            cache->rule_actions[matched_index] = parsed;
        }
        return cache->rule_actions[matched_index];
    }

    /* Default action */
    if (cache->default_action.type == RBH_ACTION_UNSET) {
        parsed = rbh_pe_parse_action(policy->action);
        parameters = policy->parameters;

        parsed.params.sstack = NULL;
        parsed.params.initialized = false;

        if (parameters) {
            parsed.params.sstack = rbh_sstack_new(1 << 10);
            if (!parsed.params.sstack)
                error(EXIT_FAILURE, errno, "rbh_sstack_new failed");

            if (!rbh_action_parameters2value_map(parameters,
                                                 &parsed.params.map,
                                                 parsed.params.sstack)) {
                rbh_sstack_destroy(parsed.params.sstack);
                parsed.params.sstack = NULL;
            } else
                parsed.params.initialized = true;
        }
        cache->default_action = parsed;
    }
    return cache->default_action;
}

/**
 * Delete an entry from the filesystem by dispatching a DELETE action through
 * the backend's apply_action operation.
 *
 * @param action       the action to apply (contains parsed parameters)
 * @param mi_backend   the mirror backend
 * @param fs_backend   the filesystem backend to use for deletion
 * @param entry        the fsentry to delete
 * @param common_ops   the backend's common operations interface
 *
 * @return 0 on success, -1 on error (errno is set)
 */
static int
rbh_pe_delete_action(const struct rbh_action *action,
                     struct rbh_backend *mi_backend,
                     struct rbh_backend *fs_backend,
                     struct rbh_fsentry *entry,
                     const struct rbh_pe_common_operations *common_ops)
{
    int rc;

    if (!entry) {
        fprintf(stderr, "DeleteAction | entry is NULL\n");
        return -1;
    }

    rc = rbh_pe_common_ops_apply_action(common_ops, action, entry, mi_backend,
                                        fs_backend);
    if (rc == 0 || rc == 1) {
        printf("DeleteAction | deleted '%s'\n", fsentry_relative_path(entry));
        if (rc == 1)
            printf("DeleteAction | removed empty parent directories\n");
        rc = 0;
    } else
        fprintf(stderr, "DeleteAction | failed to delete '%s': %s\n",
                fsentry_relative_path(entry), rbh_strerror(errno));

    return rc;
}

/**
 * Execute an external command action on a filesystem entry.
 *
 * Substitutes "{}" placeholders in the command template with the entry's
 * absolute path and executes the resulting command synchronously.
 *
 * @param action        the parsed CMD action (action->value is the command
 *                      template)
 * @param mi_backend    the mirror backend
 * @param entry         the fsentry to act on
 *
 * @return        0 on success, -1 on error
 */
static int
rbh_pe_cmd_action(const struct rbh_action *action,
                  struct rbh_fsentry *entry,
                  struct rbh_backend *mi_backend)
{
    const char *rel_path;
    int rc;

    if (!entry) {
        fprintf(stderr, "CmdAction | entry is NULL\n");
        return -1;
    }

    if (!action->value || !*action->value) {
        fprintf(stderr, "CmdAction | empty command\n");
        errno = EINVAL;
        return -1;
    }

    rel_path = fsentry_absolute_path(mi_backend, entry);

    rc = rbh_action_exec_command(action->value, rel_path);
    if (rc != 0) {
        fprintf(stderr, "CmdAction | command failed (rc=%d) for '%s': %s\n",
                rc, rel_path, action->value);
        return -1;
    }

    return 0;
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
int
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
        return rbh_pe_delete_action(action, mi_backend, fs_backend, entry,
                                    common_ops);
    case RBH_ACTION_CMD:
        return rbh_pe_cmd_action(action, entry, mi_backend);
    case RBH_ACTION_PYTHON:
        break;
    default:
        printf("Action type not supported\n");
        errno = ENOTSUP;
        return -1;
    }
    return 0;
}
