/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static bool
insert_source(struct sqlite_backend *sqlite,
              const char *plugin,
              const char **extensions,
              size_t n_extensions)
{
    const char *query =
        "insert into info (id, plugin, extensions) values (1, ?, ?) "
        "on conflict(id) do "
        "update set plugin=excluded.plugin, extensions=excluded.extensions";
    const char *extensions_array;
    struct sqlite_cursor cursor;
    int save_errno;
    json_t *array;
    bool res;

    array = sqlite_list2array(extensions, n_extensions);
    if (!array)
        return false;

    extensions_array = json_dumps(array, JSON_COMPACT);
    json_decref(array);
    if (!extensions_array)
        return false;

    res = sqlite_cursor_setup(sqlite, &cursor) &&
        sqlite_setup_query(&cursor, query) &&
        sqlite_cursor_bind_string(&cursor, plugin) &&
        sqlite_cursor_bind_string(&cursor, extensions_array) &&
        sqlite_cursor_exec(&cursor);

    save_errno = errno;
    free((char *)extensions_array);
    errno = save_errno;

    return res;
}

static bool
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

    if (!insert_source(sqlite, plugin, extensions, n_extensions))
        goto free_extensions;

    free(extensions);
    return true;

free_extensions:
    save_errno = errno;
    free(extensions);
    errno = save_errno;

    return false;
}

static bool
store_mountpoint(struct sqlite_backend *sqlite, const char *mountpoint)
{
    const char *query =
        "insert into info (id, mountpoint) values (1, ?) on conflict(id) do "
        "update set mountpoint=excluded.mountpoint";
    struct sqlite_cursor cursor;

    return sqlite_cursor_setup(sqlite, &cursor) &&
        sqlite_setup_query(&cursor, query) &&
        sqlite_cursor_bind_string(&cursor, mountpoint) &&
        sqlite_cursor_exec(&cursor);
}

static bool
insert_info(void *backend, const struct rbh_value_map *map)
{
    for (size_t i = 0; i < map->count; i++) {
        const struct rbh_value_pair *pair = &map->pairs[0];

        if (!strcmp(pair->key, "backend_source") &&
            pair->value->type == RBH_VT_SEQUENCE) {
            if (!store_source(backend, pair->value))
                return false;

        } else if (!strcmp(pair->key, "mountpoint") &&
                   pair->value->type == RBH_VT_STRING) {
            if (!store_mountpoint(backend, pair->value->string))
                return false;
        }
    }

    return true;
}

static bool
insert_log(void *backend, const struct rbh_value_map *map)
{
    return true;
}

int
sqlite_backend_insert_metadata(void *backend,
                               const struct rbh_value_map *map,
                               enum metadata_type type)
{
    switch (type) {
    case RBH_DT_INFO:
        if (!insert_info(backend, map))
            return -1;
        break;
    case RBH_DT_LOG:
        if (!insert_log(backend, map))
            return -1;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    return 0;
}

static struct rbh_value POSIX_STRING = {
    .type = RBH_VT_STRING,
    .string = "posix",
};

static struct rbh_value LUSTRE_STRING = {
    .type = RBH_VT_STRING,
    .string = "lustre",
};

static struct rbh_value RETENTION_STRING = {
    .type = RBH_VT_STRING,
    .string = "retention",
};

static struct rbh_value EXTENSION_STRING = {
    .type = RBH_VT_STRING,
    .string = "extension",
};

static struct rbh_value PLUGIN_STRING = {
    .type = RBH_VT_STRING,
    .string = "plugin",
};

static const struct rbh_value_pair POSIX_MAP_VALUES[2] = {
    { .key = "type", .value = &PLUGIN_STRING },
    { .key = "plugin", .value = &POSIX_STRING },
};

static const struct rbh_value_pair LUSTRE_MAP_VALUES[3] = {
    { .key = "type", .value = &EXTENSION_STRING },
    { .key = "plugin", .value = &POSIX_STRING },
    { .key = "extension", .value = &LUSTRE_STRING },
};

static const struct rbh_value_pair RETENTION_MAP_VALUES[3] = {
    { .key = "type", .value = &EXTENSION_STRING },
    { .key = "plugin", .value = &POSIX_STRING },
    { .key = "extension", .value = &RETENTION_STRING },
};

static struct rbh_value VALUES[3] = {
    {
        .type = RBH_VT_MAP,
        .map = {
            .count = 2,
            .pairs = POSIX_MAP_VALUES,
        }
    },
    {
        .type = RBH_VT_MAP,
        .map = {
            .count = 3,
            .pairs = LUSTRE_MAP_VALUES,
        }
    },
    {
        .type = RBH_VT_MAP,
        .map = {
            .count = 3,
            .pairs = RETENTION_MAP_VALUES,
        }
    },
};

static struct rbh_value POSIX_VALUE = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .count = 3,
        .values = VALUES,
    },
};

static struct rbh_value_pair POSIX_BACKEND = {
    .key = "backend_source",
    .value = &POSIX_VALUE,
};

static struct rbh_value_map POSIX_SOURCE = {
    .count = 1,
    .pairs = &POSIX_BACKEND,
};

struct rbh_value_map *
sqlite_backend_get_info(void *backend, int flags)
{
    if (flags & RBH_INFO_BACKEND_SOURCE)
        return &POSIX_SOURCE;

    return NULL;
}
