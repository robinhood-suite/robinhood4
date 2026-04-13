/* This file is part of RobinHood
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
#include "robinhood/policyengine/python.h"
#include "robinhood/action.h"
#include "robinhood/fsentry.h"
#include "robinhood/plugins/common_ops.h"
#include <robinhood.h>

static bool
rbh_pe_action_provider_hint(const struct rbh_action *action,
                            const char **hint, size_t *hint_len);

static bool
rbh_pe_name_matches_hint(const char *name, const char *hint, size_t hint_len);

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
    const char *colon;

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
        /* value holds either "func" or "module:func" */
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

    /*
     * "<ext>:<builtin>": extension-namespaced built-in action.
     * value points to the full action string so that
     * rbh_pe_select_common_ops_for_actions can extract the extension hint.
     * e.g. "lustre:delete", "retention:log"
     */
    colon = strchr(action_str, ':');
    if (colon) {
        const char *sub = colon + 1;
        if (strcmp(sub, "delete") == 0)
            act.type = RBH_ACTION_DELETE;
        else if (strcmp(sub, "log") == 0)
            act.type = RBH_ACTION_LOG;

        if (act.type == RBH_ACTION_DELETE || act.type == RBH_ACTION_LOG) {
            act.value = action_str;
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

    cache->default_count_used = 0;
    cache->rule_count_used = NULL;

    cache->rule_count = policy->rule_count;

    if (policy->rule_count == 0)
        return;

    cache->rule_actions = xcalloc(policy->rule_count,
                                  sizeof(*cache->rule_actions));

    cache->rule_count_used = xcalloc(policy->rule_count,
                                     sizeof(*cache->rule_count_used));

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
    if (cache->rule_actions)
        free(cache->rule_actions);
    cache->rule_actions = NULL;

    if (cache->rule_count_used)
        free(cache->rule_count_used);
    cache->rule_count_used = NULL;

    cache->rule_count = 0;
    cache->default_count_used = 0;
}

/**
 * Initialize the parameters of a parsed action.
 *
 * Converts the YAML parameter string into a rbh_value_map using the global
 * context allocation. Memory persists until program exit.
 *
 * @param parsed      the action whose params field will be filled
 * @param parameters  YAML parameter string, or NULL
 */
static void
rbh_pe_load_action_params(struct rbh_action *parsed, const char *parameters)
{
    /* Initialize to empty map */
    parsed->params.generic.pairs = NULL;
    parsed->params.generic.count = 0;

    if (!parameters)
        return;

    /* Parse YAML into value_map using global context */
    if (!rbh_action_parameters2value_map(parameters, &parsed->params.generic)) {
        parsed->params.generic.pairs = NULL;
        parsed->params.generic.count = 0;
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
            const struct rbh_rule *rule = &policy->rules[matched_index];

            parsed = rbh_pe_parse_action(rule->action);
            if (parsed.type == RBH_ACTION_DELETE) {
                parsed.params.delete = rule->parameters.delete;
            } else if (parsed.type == RBH_ACTION_LOG) {
                parsed.params.log = rule->parameters.log;
            } else {
                parameters = rule->parameters.generic;
                rbh_pe_load_action_params(&parsed, parameters);
            }

            cache->rule_actions[matched_index] = parsed;
        }
        return cache->rule_actions[matched_index];
    }

    /* Default action */
    if (cache->default_action.type == RBH_ACTION_UNSET) {
        parsed = rbh_pe_parse_action(policy->action);
        if (parsed.type == RBH_ACTION_DELETE) {
            parsed.params.delete = policy->parameters.delete;
        } else if (parsed.type == RBH_ACTION_LOG) {
            parsed.params.log = policy->parameters.log;
        } else {
            parameters = policy->parameters.generic;
            rbh_pe_load_action_params(&parsed, parameters);
        }

        cache->default_action = parsed;
    }
    return cache->default_action;
}

size_t
rbh_pe_action_count_limit(const struct rbh_action *action)
{
    switch (action->type) {
    case RBH_ACTION_DELETE:
        return action->params.delete.count;
    case RBH_ACTION_LOG:
        return action->params.log.count;
    default:
        return 0;
    }
}

static const struct rbh_pe_common_operations *
rbh_pe_select_common_ops_for_actions(const struct rbh_action *action,
                                     const struct filters_context *f_ctx)
{
    const struct rbh_pe_common_operations *plugin_ops = NULL;
    const struct rbh_pe_common_operations *ext_ops = NULL;
    const struct rbh_backend_plugin *plugin = NULL;
    bool plugin_hint_matches = false;
    const char *ext_hint = NULL;
    size_t ext_hint_len = 0;
    size_t ext_matches = 0;

    rbh_pe_action_provider_hint(action, &ext_hint, &ext_hint_len);

    for (size_t i = 0; i < f_ctx->info_pe_count; ++i) {
        const struct rbh_plugin_extension *ext;

        if (f_ctx->info_pe[i].is_plugin) {
            plugin = f_ctx->info_pe[i].plugin;

            if (plugin && plugin->common_ops)
                plugin_ops = plugin->common_ops;

            continue;
        }

        ext = f_ctx->info_pe[i].extension;

        if (!ext || !ext->common_ops || !ext->common_ops->delete_entry)
            continue;

        /* If the user specified an extension hint ("lustre:log"), skip any
         * extension whose name does not match. */
        if (ext_hint != NULL &&
            !rbh_pe_name_matches_hint(ext->name, ext_hint, ext_hint_len))
            continue;

        if (ext_matches > 0) {
            fprintf(stderr, "Error: multiple extensions implement action %s. "
                    "Specify the extension explicitly in the configuration "
                    "(e.g. 'lustre.delete' instead of 'action.delete').\n",
                    action2string(action->type));
            errno = EINVAL;
            return NULL;
        }

        ext_ops = ext->common_ops;
        ext_matches++;
    }

    if (plugin && plugin_ops && plugin_ops->delete_entry) {
        if (ext_hint == NULL) {
            plugin_hint_matches = true;
        } else if (rbh_pe_name_matches_hint(plugin->plugin.name, ext_hint,
                                            ext_hint_len)) {
            plugin_hint_matches = true;
        }
    }

    if (ext_hint != NULL) {
        char hint_str[256] = {0};
        size_t copy_len;

        if (ext_matches == 1 && plugin_hint_matches) {
            fprintf(stderr,
                    "Error: both backend plugin '%s' and an extension "
                    "implement action %s\n",
                    plugin->plugin.name, action2string(action->type));
            errno = EINVAL;
            return NULL;
        }

        if (ext_matches == 1)
            return ext_ops;

        if (plugin_hint_matches)
            return plugin_ops;

        /* The user named a provider that does not implement this action. */
        copy_len = ext_hint_len < sizeof(hint_str) - 1
                   ? ext_hint_len : sizeof(hint_str) - 1;
        memcpy(hint_str, ext_hint, copy_len);
        fprintf(stderr,
                "Error: plugin or extension '%s' does not implement action %s\n",
                hint_str, action2string(action->type));
        errno = ENOTSUP;
        return NULL;
    }

    if (ext_matches == 1)
        return ext_ops;

    if (plugin_hint_matches)
        return plugin_ops;

    fprintf(stderr, "Error: action %s not supported by backend\n",
            action2string(action->type));
    errno = ENOTSUP;
    return NULL;
}

static bool
rbh_pe_action_provider_hint(const struct rbh_action *action,
                            const char **hint, size_t *hint_len)
{
    const char *colon;

    *hint = NULL;
    *hint_len = 0;

    if (!action || !action->value)
        return false;

    colon = strchr(action->value, ':');
    if (!colon)
        return false;

    *hint = action->value;
    *hint_len = (size_t)(colon - action->value);
    return true;
}

static bool
rbh_pe_name_matches_hint(const char *name, const char *hint, size_t hint_len)
{
    return name != NULL && strncmp(name, hint, hint_len) == 0 &&
           name[hint_len] == '\0';
}

/*
 * Log an entry by formatting provided values.
 *
 * @param action       the action to apply (contains parsed parameters)
 * @param entry        the fsentry to log
 * @param f_ctx        context containing loaded plugins/extensions
 *
 * @return 0 on success, -1 on error (errno is set)
 */
static int
rbh_pe_log_action(const struct rbh_action *action,
                  struct rbh_fsentry *entry,
                  const struct filters_context *f_ctx)
{
    const char *format = action->params.log.format;
    char details[4096];
    int rc;

    if (!entry || !f_ctx)
        return -1;

    if (!format) {
        fprintf(stderr, "Error: log action requires a 'format' parameter\n");
        errno = EINVAL;
        return -1;
    }

    rc = rbh_action_format_fsentry(format, f_ctx, entry, "",
                                   details, sizeof(details));
    if (rc < 0) {
        fprintf(stderr, "Error: failed to format log action output\n");
        errno = EINVAL;
        return -1;
    }

    printf("LogAction | %s\n", details);

    return 0;
}

/**
 * Delete an entry from the filesystem/object store by calling the associated
 * action via the commons_ops of the backend.
 *
 * @param action       the action to apply (contains parsed parameters)
 * @param mi_backend   the mirror backend
 * @param entry        the fsentry to delete
 * @param common_ops   the backend's common operations interface
 *
 * @return 0 on success, -1 on error (errno is set)
 */
static int
rbh_pe_delete_action(const struct rbh_action *action,
                     struct rbh_backend *mi_backend,
                     struct rbh_fsentry *entry,
                     const struct rbh_pe_common_operations *common_ops)
{
    int rc;

    if (!entry) {
        fprintf(stderr, "DeleteAction | entry is NULL\n");
        return -1;
    }

    rc = rbh_pe_common_ops_delete_entry(common_ops, mi_backend, entry,
                                        &action->params.delete);
    if (rc ==  RBH_DELETE_OK || rc == RBH_DELETE_OK_WITH_PARENTS) {
        printf("DeleteAction | deleted '%s'\n", fsentry_relative_path(entry));
        if (rc == RBH_DELETE_OK_WITH_PARENTS)
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
 * @param entry         the fsentry to act on
 * @param mi_backend    the mirror backend
 *
 * @return              0 on success, -1 on error
 */
static int
rbh_pe_cmd_action(const struct rbh_action *action,
                  struct rbh_fsentry *entry,
                  struct rbh_backend *mi_backend)
{
    const char *abs_path;
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

    abs_path = fsentry_absolute_path(mi_backend, entry);

    rc = rbh_action_exec_command(action->value, abs_path);
    if (rc != 0) {
        fprintf(stderr, "CmdAction | command failed (rc=%d) for '%s': %s\n",
                rc, abs_path, action->value);
        return -1;
    }

    return 0;
}

/**
 * Apply an action to a filesystem entry.
 *
 * This function dispatches the action based on its type.
 *
 * @param action        the action to apply (contains parsed parameters)
 * @param entry         the fsentry on which the action is applied
 * @param mi_backend    the mirror backend
 * @param fs_backend    the filesystem backend
 * @param f_ctx         context containing plugins/extensions
 *
 * @return              0 on success, -1 on error
 */
int
rbh_pe_apply_action(const struct rbh_action *action,
                    struct rbh_fsentry *entry,
                    struct rbh_backend *mi_backend,
                    struct rbh_backend *fs_backend,
                    const struct filters_context *f_ctx)
{
    const struct rbh_pe_common_operations *common_ops;

    switch (action->type) {
    case RBH_ACTION_LOG:
        return rbh_pe_log_action(action, entry, f_ctx);
    case RBH_ACTION_DELETE:
        common_ops = rbh_pe_select_common_ops_for_actions(action, f_ctx);
        if (!common_ops)
            return -1;

        return rbh_pe_delete_action(action, mi_backend, entry, common_ops);
    case RBH_ACTION_CMD:
        return rbh_pe_cmd_action(action, entry, mi_backend);
    case RBH_ACTION_PYTHON:
        return rbh_pe_python_action(action, entry, mi_backend);
    default:
        printf("Action type not supported\n");
        errno = ENOTSUP;
        return -1;
    }
}
