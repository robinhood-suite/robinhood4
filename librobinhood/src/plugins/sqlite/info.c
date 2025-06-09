/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

/*----------------------------------------------------------------------------*
 |                            insert_source()                                 |
 *----------------------------------------------------------------------------*/

static int
insert_source(struct sqlite_backend *sqlite,
              const char *plugin,
              const char **extensions,
              size_t n_extensions)
{
    const char *query =
        "insert or replace into info (id, plugin, extensions) values (1, ?, ?)";
    const char *extensions_array;
    struct sqlite_cursor cursor;
    int save_errno;
    json_t *array;
    bool res;

    array = sqlite_list2array(extensions, n_extensions);
    if (!array)
        return -1;

    extensions_array = json_dumps(array, JSON_COMPACT);
    json_decref(array);
    if (!extensions_array)
        return -1;

    res = sqlite_cursor_setup(sqlite, &cursor) &&
        sqlite_setup_query(&cursor, query) &&
        sqlite_cursor_bind_string(&cursor, plugin) &&
        sqlite_cursor_bind_string(&cursor, extensions_array) &&
        sqlite_cursor_exec(&cursor);

    save_errno = errno;
    free((char *)extensions_array);
    errno = save_errno;

    return res ? 0 : -1;
}

static int
store_source(struct sqlite_backend *sqlite, const struct rbh_value *sequence)
{
    const char **extensions = NULL;
    const char *plugin = NULL;
    size_t n_extensions = 0;
    int save_errno;

    for (size_t i = 0; i < sequence->sequence.count; i++) {
        const struct rbh_value *map = &sequence->sequence.values[i];
        bool is_plugin;

        if (map->type != RBH_VT_MAP) {
            errno = EINVAL;
            goto free_extensions;
        }

        is_plugin = (map->map.count == 2);
        for (size_t j = 0; j < map->map.count; j++) {
            const struct rbh_value_pair *attr = &map->map.pairs[j];

            if (attr->value->type != RBH_VT_STRING) {
                errno = EINVAL;
                goto free_extensions;
            }

            if (!strcmp(attr->key, "type")) {
                if ((is_plugin && strcmp(attr->value->string, "plugin")) ||
                    (!is_plugin && strcmp(attr->value->string, "extension"))) {
                    errno = EINVAL;
                    goto free_extensions;
                }

            }

            if (is_plugin && !strcmp(attr->key, "plugin")) {
                plugin = attr->value->string;
                break;
            }

            if (!is_plugin && !strcmp(attr->key, "extension")) {
                if (!extensions) {
                    extensions = malloc(sizeof(*extensions) * 16);
                    if (!extensions)
                        goto free_extensions;
                }

                extensions[n_extensions++] = attr->value->string;
                break;
            }
        }
    }

    if (insert_source(sqlite, plugin, extensions, n_extensions) == -1)
        goto free_extensions;

    free(extensions);
    return 0;

free_extensions:
    save_errno = errno;
    free(extensions);
    errno = save_errno;

    return -1;
}

int
sqlite_backend_insert_source(void *backend,
                             const struct rbh_value *source)
{
    switch (source->type) {
    case RBH_VT_SEQUENCE:
        return store_source(backend, source);
    default:
        errno = EINVAL;
        return -1;
    }
    return 0;
}

/*----------------------------------------------------------------------------*
 |                            get_info()                                      |
 *----------------------------------------------------------------------------*/

static json_t *
plugin2json(const char *plugin)
{
    json_t *object = json_object();

    json_object_set_new(object, "type", json_string("plugin"));
    json_object_set_new(object, "plugin", json_string(plugin));

    return object;
}

static json_t *
extension2json(const char *plugin, const char *extension)
{
    json_t *object = json_object();

    json_object_set_new(object, "type", json_string("extension"));
    json_object_set_new(object, "plugin", json_string(plugin));
    json_object_set_new(object, "extension", json_string(extension));

    return object;
}

static json_t *
source2json(const char *plugin, const char *extensions_json)
{
    json_t *backend_source;
    json_t *extensions;
    json_error_t err;
    json_t *source;
    json_t *value;
    size_t i;

    extensions = json_loads(extensions_json, 0, &err);
    if (!extensions)
        return NULL;

    source = json_array();
    if (!source)
        goto free_extensions;

    json_array_append_new(source, plugin2json(plugin));

    json_array_foreach(extensions, i, value) {
        json_t *extension;

        if (!json_is_string(value))
            goto free_extensions;

        extension = extension2json(plugin, json_string_value(value));
        json_array_append_new(source, extension);
    }

    backend_source = json_object();
    json_object_set_new(backend_source, "backend_source", source);

    json_decref(extensions);
    return backend_source;

free_extensions:
    json_decref(extensions);
    return NULL;
}

static json_t *
backend_source(struct sqlite_backend *sqlite)
{
    const char *query = "select plugin, extensions from info where id = 1";
    struct sqlite_cursor cursor;
    const char *extensions;
    const char *plugin;
    json_t *source;

    if (!(sqlite_cursor_setup(sqlite, &cursor) &&
        sqlite_setup_query(&cursor, query) &&
        sqlite_cursor_step(&cursor)))
        return NULL;

    plugin = sqlite_cursor_get_string(&cursor);
    extensions = sqlite_cursor_get_string(&cursor);

    source = source2json(plugin, extensions);
    sqlite_cursor_fini(&cursor);

    return source;
}

struct rbh_value_map *
sqlite_backend_get_info(void *backend, int flags)
{
    struct sqlite_backend *sqlite = backend;
    struct rbh_value_map *map;
    json_t *info;

    map = malloc(sizeof(*map));
    if (!map)
        return NULL;

    if (flags & RBH_INFO_BACKEND_SOURCE)
        info = backend_source(sqlite);

    if (!json2value_map(info, map, sqlite->sstack))
        goto free_map;

    json_decref(info);
    return map;

free_map:
    free(map);
    return NULL;
}
