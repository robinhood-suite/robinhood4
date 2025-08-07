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
#include "robinhood/statx.h"
#include <robinhood.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

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
    bool is_plugin = true;

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

struct rbh_backend_plugin_info
get_backend_plugin_info(const char *uri)
{
    struct rbh_backend_plugin_info info = {0};

    rbh_config_load_from_path(NULL);
    struct rbh_backend *backend = rbh_backend_from_uri(uri, true);
    if (!backend)
        error(EXIT_FAILURE, errno, "rbh_backend_from_uri");

    const struct rbh_value_map *info_map =
        rbh_backend_get_info(backend, RBH_INFO_BACKEND_SOURCE);
    if (!info_map)
        error(EXIT_FAILURE, errno, "rbh_backend_get_info failed");

    assert(info_map->count == 1);
    assert(strcmp(info_map->pairs[0].key, "backend_source") == 0);
    const struct rbh_value *sequence_value = info_map->pairs[0].value;
    assert(sequence_value->type == RBH_VT_SEQUENCE);

    const struct rbh_value *entries = sequence_value->sequence.values;
    size_t entry_count = sequence_value->sequence.count;

    const char *plugin_name = NULL;
    const char **extension_names = NULL;
    int extension_count = 0;

    for (size_t i = 0; i < entry_count; ++i) {
        const struct rbh_value *entry = &entries[i];
        assert(entry->type == RBH_VT_MAP);
        const struct rbh_value_map *entry_map = &entry->map;

        const struct rbh_value *type_value = NULL;
        const struct rbh_value *plugin_value = NULL;
        const struct rbh_value *extension_value = NULL;

        for (size_t j = 0; j < entry_map->count; ++j) {
            const struct rbh_value_pair *pair = &entry_map->pairs[j];

            if (strcmp(pair->key, "type") == 0)
                type_value = pair->value;
            else if (strcmp(pair->key, "plugin") == 0)
                plugin_value = pair->value;
            else if (strcmp(pair->key, "extension") == 0)
                extension_value = pair->value;
        }

        assert(plugin_value != NULL);
        assert(plugin_value->type == RBH_VT_STRING);

        if (type_value && strcmp(type_value->string, "plugin") == 0) {
            plugin_name = plugin_value->string;
        } else if (extension_value && extension_value->type == RBH_VT_STRING) {
            extension_names = realloc(extension_names,
                                      sizeof(char *) * (extension_count + 1));
            if (!extension_names)
                error(EXIT_FAILURE, errno, "realloc");
            extension_names[extension_count++] = extension_value->string;
        }
    }

    if (!plugin_name)
        error(EXIT_FAILURE, 0, "plugin name not found in backend source");

    info.plugin = rbh_backend_plugin_import(plugin_name);
    if (!info.plugin)
        error(EXIT_FAILURE, errno, "rbh_backend_plugin_import");

    if (extension_count > 0) {
        info.extensions = malloc(
            sizeof(struct rbh_plugin_extension *) * extension_count);
        if (!info.extensions)
            error(EXIT_FAILURE, errno, "malloc");

        for (int i = 0; i < extension_count; ++i) {
            const struct rbh_plugin_extension *ext =
                rbh_plugin_load_extension(&info.plugin->plugin,
                                          extension_names[i]);
            if (!ext)
                error(EXIT_FAILURE, errno, "rbh_plugin_load_extension");

            info.extensions[i] = ext;
        }
    }

    info.extension_count = extension_count;
    free(extension_names);

    return info;
}

struct rbh_filter *
build_filter_from_uri(const char *uri, const char **argv)
{
    struct rbh_backend_plugin_info info = get_backend_plugin_info(uri);

    struct rbh_filter *filter = NULL;
    int argc = 2;
    int index = 0;
    bool need_prefetch = false;

    if (info.plugin->common_ops && info.plugin->common_ops->build_filter) {
        int pipefd[2];
        if (pipe(pipefd) != -1) {
            pid_t pid = fork();
            if (pid == 0) {
                close(pipefd[0]);
                struct rbh_filter *f =
                    info.plugin->common_ops->build_filter(argv, argc, &index,
                                                          &need_prefetch);
                if (f != NULL) {
                    write(pipefd[1], "1", 1);
                }
                close(pipefd[1]);
                _exit(0);
            }

            close(pipefd[1]);
            char result = 0;
            read(pipefd[0], &result, 1);
            close(pipefd[0]);

            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status) && result == '1') {
                filter = info.plugin->common_ops->build_filter(argv, argc,
                                                               &index,
                                                               &need_prefetch);
                if (filter != NULL)
                    return filter;
            }
        }
    }


    for (int i = 0; i < info.extension_count; ++i) {
        const struct rbh_plugin_extension *ext = info.extensions[i];

        if (ext->common_ops && ext->common_ops->build_filter) {
            int pipefd[2];
            if (pipe(pipefd) == -1)
                continue;

            pid_t pid = fork();
            if (pid == -1) {
                close(pipefd[0]);
                close(pipefd[1]);
                continue;
            }

            if (pid == 0) {
                close(pipefd[0]);
                struct rbh_filter *f =
                    ext->common_ops->build_filter(argv, argc, &index,
                                                  &need_prefetch);
                if (f != NULL) {
                    write(pipefd[1], "1", 1);
                }
                close(pipefd[1]);
                _exit(0);
            }

            close(pipefd[1]);
            char result = 0;
            read(pipefd[0], &result, 1);
            close(pipefd[0]);

            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status) && result == '1') {
                filter = ext->common_ops->build_filter(argv, argc, &index,
                                                       &need_prefetch);
                if (filter != NULL)
                    return filter;
            }
        }
    }

    return NULL;
}

static int
complete_logical_filter(const struct rbh_filter *filter,
                        struct rbh_backend *backend,
                        const struct rbh_filter_options *options,
                        const struct rbh_filter_output *output)
{
    for (uint32_t i = 0; i < filter->logical.count; i++) {
        if (complete_rbh_filter(filter->logical.filters[i], backend, options,
                                output))
            return -1;
    }

    return 0;
}

static void
update_statx_rbh_value(struct rbh_filter *filter,
                       const struct rbh_filter_field *field,
                       const struct rbh_statx *statx)
{
    switch (field->statx) {
    case RBH_STATX_MTIME_SEC:
        filter->compare.value.uint64 = statx->stx_mtime.tv_sec;
        return;
    default:
        return;
    }
}

static void
update_rbh_value(struct rbh_filter *filter,
                 const struct rbh_filter_field *field,
                 struct rbh_fsentry *fsentry)
{
    switch (field->fsentry) {
    case RBH_FP_STATX:
        return update_statx_rbh_value(filter, field, fsentry->statx);
    default:
        return;
    }
}

static int
complete_get_filter(const struct rbh_filter *filter,
                    struct rbh_backend *backend,
                    const struct rbh_filter_options *options,
                    const struct rbh_filter_output *output)
{
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *fsentry;

    fsentries = rbh_backend_filter(backend, filter->get.fsentry_to_get, options,
                                   output);
    if (fsentries == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "filter_fsentries");

    fsentry = rbh_mut_iter_next(fsentries);
    if (fsentry == NULL) {
        rbh_mut_iter_destroy(fsentries);
        return -1;
    }

    update_rbh_value(filter->get.filter, &filter->get.field, fsentry);

    free(fsentry);
    rbh_mut_iter_destroy(fsentries);

    return 0;
}

int
complete_rbh_filter(const struct rbh_filter *filter,
                    struct rbh_backend *backend,
                    const struct rbh_filter_options *options,
                    const struct rbh_filter_output *output)
{
    if (rbh_is_logical_operator(filter->op))
        return complete_logical_filter(filter, backend, options, output);

    if (rbh_is_get_operator(filter->op))
        return complete_get_filter(filter, backend, options, output);

    return 0;
}
