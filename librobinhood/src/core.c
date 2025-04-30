/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/filters/core.h"

void
filters_ctx_finish(struct filters_context *ctx)
{
    free(ctx->info_pe);
}

static bool
check_pe_already_registered(struct filters_context *ctx, int pe_count,
                            const char *pe_string)
{
    for (int i = 0; i < pe_count; ++i) {
        if (ctx->info_pe[i].is_plugin &&
            strcmp(ctx->info_pe[i].plugin->plugin.name, pe_string) == 0)
            return true;
        else if (!ctx->info_pe[i].is_plugin &&
                 strcmp(ctx->info_pe[i].extension->name, pe_string) == 0)
            return true;
    }

    return false;
}

static int
import_backend_source(struct filters_context *ctx,
                      int pe_count, const struct rbh_value_map *backend_source)
{
    const struct rbh_value *extension_value = NULL;
    const struct rbh_value *plugin_value = NULL;
    const struct rbh_backend_plugin *plugin;
    bool is_plugin;

    for (int i = 0; i < backend_source->count; ++i) {
        const struct rbh_value_pair *pair = &backend_source->pairs[i];

        assert(pair->value->type == RBH_VT_STRING);

        if (strcmp(pair->key, "type") == 0)
            is_plugin = (strcmp(pair->value->string, "plugin") == 0);
        else if (strcmp(pair->key, "plugin") == 0)
            plugin_value = pair->value;
        else if (strcmp(pair->key, "extension") == 0)
            extension_value = pair->value;
    }

    assert(plugin_value != NULL);

    if (is_plugin && check_pe_already_registered(ctx, pe_count,
                                                 plugin_value->string))
        return 0;
    else if (!is_plugin && check_pe_already_registered(ctx, pe_count,
                                                       extension_value->string))
        return 0;

    plugin = rbh_backend_plugin_import(plugin_value->string);
    if (plugin == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_plugin_import");

    if (is_plugin) {
        ctx->info_pe[pe_count].plugin = plugin;
        ctx->info_pe[pe_count].is_plugin = true;
    } else {
        ctx->info_pe[pe_count].is_plugin = false;
        ctx->info_pe[pe_count].extension = rbh_plugin_load_extension(
            &plugin->plugin,
            extension_value->string
        );
        if (ctx->info_pe[pe_count].extension == NULL)
            error(EXIT_FAILURE, errno, "rbh_plugin_load_extension");
    }

    return 1;
}

void
import_plugins(struct filters_context *ctx, struct rbh_value_map **info_maps,
               int backend_count)
{
    int pe_count = 0;

    for (int i = 0; i < backend_count; ++i) {
        assert(info_maps[i]->count == 1);
        assert(strcmp(info_maps[i]->pairs->key, "backend_source") == 0);
        assert(info_maps[i]->pairs[0].value->type == RBH_VT_SEQUENCE);

        ctx->info_pe_count += info_maps[i]->pairs[0].value->sequence.count;
    }

    ctx->info_pe = malloc(ctx->info_pe_count * sizeof(*ctx->info_pe));
    if (ctx->info_pe == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    for (int i = 0; i < backend_count; ++i) {
        const struct rbh_value *backend_source_sequence =
            info_maps[i]->pairs[0].value;

        for (int j = 0; j < backend_source_sequence->sequence.count; ++j) {
            const struct rbh_value *backend_source =
                &backend_source_sequence->sequence.values[j];

            assert(backend_source->type == RBH_VT_MAP);
            pe_count += import_backend_source(ctx, pe_count,
                                              &backend_source->map);
        }
    }

    ctx->info_pe_count = pe_count;
}
