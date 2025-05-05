/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FILTERS_CORE_H
#define RBH_FILTERS_CORE_H

#include <stdlib.h>

#include <robinhood.h>

struct rbh_plugin_or_extension {
    bool is_plugin;
    union {
        const struct rbh_backend_plugin *plugin;
        const struct rbh_plugin_extension *extension;
    };
};

static inline const struct rbh_pe_common_operations *
get_common_operations(struct rbh_plugin_or_extension *pe)
{
    if (pe->is_plugin)
        return pe->plugin->common_ops;
    else
        return pe->extension->common_ops;
}

/**
 * Filters library context
 */
struct filters_context {
    /** The type of information stored in the backends */
    struct rbh_plugin_or_extension *info_pe;
    size_t info_pe_count;

    /** If a filter need to be completed by an information of the backend */
    bool need_prefetch;
};

/**
 * Destroy the `struct filters_context`
 *
 * @param ctx      filters context
 */
void
filters_ctx_finish(struct filters_context *ctx);

void
import_plugins(struct filters_context *ctx, struct rbh_value_map **info_maps,
               int backend_count);

#endif
