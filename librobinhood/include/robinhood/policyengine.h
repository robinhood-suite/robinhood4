/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_PE
#define RBH_PE

#include "robinhood/filters/core.h"
#include "robinhood/fsentry.h"

struct rbh_rule {
    const char *name;
    struct rbh_filter *filter;
    const char *action;
    const char *parameters;
};

struct rbh_policy {
    const char *name;
    struct rbh_filter *filter;
    const char *action;
    const char *parameters;
    struct rbh_rule *rules;
    size_t rule_count;
};

struct rbh_mut_iterator *
rbh_collect_fsentries(struct rbh_backend *backend, struct rbh_filter *filter);

int
rbh_pe_execute(struct rbh_mut_iterator *mongo_iter,
               struct rbh_backend *mirror_backend,
               const char *fs_uri,
               const struct rbh_policy *policy);

#endif
