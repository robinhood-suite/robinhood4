/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_POLICYENGINE_CORE_H
#define RBH_POLICYENGINE_CORE_H

#include "robinhood/filters/core.h"
#include "robinhood/fsentry.h"
#include "robinhood/action.h"

/**
 * rbh_action_cache — caches parsed actions for a policy execution.
 *
 * Each action string (default policy action or per-rule action) is parsed only
 * once. The cache stores the parsed result so that repeated matches of the
 * same rule do not trigger repeated parsing.
 */
struct rbh_action_cache {
    struct rbh_action default_action;
    struct rbh_action *rule_actions;
    size_t rule_count;

    size_t default_count_used;
    size_t *rule_count_used;
};

struct rbh_rule {
    const char *name;
    struct rbh_filter *filter;
    const char *action;

    union {
        const char *generic;               // YAML
        struct rbh_delete_params delete;
        struct rbh_log_params log;
    } parameters;
};

struct rbh_policy {
    const char *name;
    struct rbh_filter *filter;
    const char *action;

    union {
        const char *generic;               // YAML
        struct rbh_delete_params delete;
        struct rbh_log_params log;
    } parameters;

    struct rbh_rule *rules;
    size_t rule_count;
};

const char *
rbh_backend_get_mountpoint(struct rbh_backend *backend);

const char *
rbh_backend_get_source_backend(struct rbh_backend *backend);

struct rbh_mut_iterator *
rbh_collect_fsentries(struct rbh_backend *backend, struct rbh_filter *filter);

int
rbh_pe_execute(struct rbh_mut_iterator *mirror_iter,
               struct rbh_backend *mirror_backend,
               const char *fs_uri,
               const struct rbh_policy *policy);

#endif
