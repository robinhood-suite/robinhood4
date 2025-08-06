/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_CORE_FILTERS_H
#define ROBINHOOD_CORE_FILTERS_H

#include "robinhood/plugins/backend.h"

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

    /** If a filter needs to be completed by an information of the backend */
    bool need_prefetch;

    int argc;
    char **argv;
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
/**
 * Struct holding information about a backend plugin and its associated
 * extensions, extracted from a backend URI.
 */
struct rbh_backend_plugin_info {
    /**
     * Pointer to the main backend plugin extracted from the URI.
     * This plugin typically provides the core operations for the backend.
     */
    const struct rbh_backend_plugin *plugin;
    /**
     * Array of pointers to plugin extensions associated with the backend
     * plugin.
     * Extensions may provide additional capabilities such as custom predicates.
     */
    const struct rbh_plugin_extension **extensions;
    int extension_count;
};

/**
 * Parse a backend URI and return the associated plugin information, including
 * the main backend plugin and its optional extensions.
 *
 * @param uri The URI representing the backend to load.
 * @return A populated rbh_backend_plugin_info struct with plugin and
 * extensions.
 */
struct rbh_backend_plugin_info
get_backend_plugin_info(const char *uri);

struct command_context {
    char *config_file;
    char *helper_target;
    bool helper;
    bool dry_run;
    bool verbose;
};

/**
 * Complete filter that need information to be fetch from a backend.
 *
 * @param  filter   the filter to complete
 * @param  backend  the backend from which to fetch the information
 * @param  options  a set of filtering options (must not be NULL)
 * @param  output   the information to be outputted
 */
int
complete_rbh_filter(const struct rbh_filter *filter,
                    struct rbh_backend *backend,
                    const struct rbh_filter_options *options,
                    const struct rbh_filter_output *output);

#endif
