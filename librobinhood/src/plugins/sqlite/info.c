/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "internals.h"

/*----------------------------------------------------------------------------*
 |                            insert_source()                                 |
 *----------------------------------------------------------------------------*/

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
insert_last_read(struct sqlite_backend *sqlite, const char *id,
                 uint64_t last_read)
{
    const char *query =
        "insert into readers (id, last_read) "
        "values (?, ?) on conflict(id) do "
        "update set last_read = excluded.last_read";
    struct sqlite_cursor cursor;

    return sqlite_cursor_setup(sqlite, &cursor) &&
        sqlite_setup_query(&cursor, query) &&
        sqlite_cursor_bind_string(&cursor, id) &&
        sqlite_cursor_bind_int64(&cursor, last_read) &&
        sqlite_cursor_exec(&cursor);
}

static bool
store_fsevents_source(struct sqlite_backend *sqlite,
                      const struct rbh_value *value)
{
    for (size_t i = 0; i < value->map.count; i++) {
        const struct rbh_value_pair *pair = &value->map.pairs[i];

        if (pair->value->type != RBH_VT_MAP ||
            pair->value->map.count != 1 ||
            strcmp(pair->value->map.pairs[0].key, "last_read") ||
            pair->value->map.pairs[0].value->type != RBH_VT_UINT64) {
            errno = EINVAL;
            return false;
        }

        if (!insert_last_read(sqlite, pair->key,
                              pair->value->map.pairs[0].value->uint64))
            return false;
    }

    return true;
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

        } else if (!strcmp(pair->key, "fsevents_source") &&
                   pair->value->type == RBH_VT_MAP) {
            if (!store_fsevents_source(backend, pair->value))
                return false;

        } else {
            errno = EINVAL;
            return false;
        }
    }

    return true;
}

static bool
insert_log(struct sqlite_backend *sqlite, const struct rbh_value_map *map)
{
    const char *query =
        "insert into log ("
        "    mountpoint, cli, duration, inserted, skipped, start, total, end"
        ") values ("
        "    ?, ?, ?, ?, ?, ?, ?, ?"
        ")";
    const char *mountpoint = NULL;
    struct sqlite_cursor cursor;
    const char *cli = NULL;
    int64_t duration = -1;
    int64_t inserted = -1;
    int64_t skipped = -1;
    int64_t start = -1;
    int64_t total = -1;
    int64_t end = -1;

    for (size_t i = 0; i < map->count; i++) {
        const struct rbh_value_pair *pair = &map->pairs[i];

        if (!strcmp(pair->key, "sync_debut"))
            start = pair->value->int64;
        else if (!strcmp(pair->key, "sync_duration"))
            duration = pair->value->int64;
        else if (!strcmp(pair->key, "sync_end"))
            end = pair->value->int64;
        else if (!strcmp(pair->key, "mountpoint"))
            mountpoint = pair->value->string;
        else if (!strcmp(pair->key, "command_line"))
            cli = pair->value->string;
        else if (!strcmp(pair->key, "converted_entries"))
            inserted = pair->value->int64;
        else if (!strcmp(pair->key, "skipped_entries"))
            skipped = pair->value->int64;
        else if (!strcmp(pair->key, "total_entries_seen"))
            total = pair->value->int64;
    }

    if (duration == -1 || inserted == -1 || skipped == -1 || total == -1 ||
        end == -1 || start == -1 || mountpoint == NULL || cli == NULL) {
        errno = EINVAL;
        return false;
    }

    return sqlite_cursor_setup(sqlite, &cursor) &&
        sqlite_setup_query(&cursor, query) &&
        sqlite_cursor_bind_string(&cursor, mountpoint) &&
        sqlite_cursor_bind_string(&cursor, cli) &&
        sqlite_cursor_bind_int64(&cursor, duration) &&
        sqlite_cursor_bind_int64(&cursor, inserted) &&
        sqlite_cursor_bind_int64(&cursor, skipped) &&
        sqlite_cursor_bind_int64(&cursor, start) &&
        sqlite_cursor_bind_int64(&cursor, total) &&
        sqlite_cursor_bind_int64(&cursor, end) &&
        sqlite_cursor_exec(&cursor);
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

static json_t *
backend_size(struct sqlite_backend *sqlite, json_t *previous_info)
{
    json_t *info = previous_info ? : json_object();
    struct stat st;

    if (stat(sqlite->path, &st) == -1)
        return NULL;

    json_object_set_new(info, "size", json_integer(st.st_size));

    return info;
}

static json_t *
backend_count(struct sqlite_backend *sqlite, json_t *previous_info)
{
    const char *query = "select count(*) from entries";
    json_t *info = previous_info ? : json_object();
    struct sqlite_cursor cursor;

    if (!(sqlite_cursor_setup(sqlite, &cursor) &&
          sqlite_setup_query(&cursor, query) &&
          sqlite_cursor_step(&cursor)))
        return NULL;

    json_object_set_new(info, "count",
                        json_integer(sqlite_cursor_get_int64(&cursor)));
    sqlite_cursor_fini(&cursor);

    return info;
}

static json_t *
backend_fsevents_source(struct sqlite_backend *sqlite, json_t *previous_info)
{
    const char *query = "select id, last_read from readers";
    json_t *info = previous_info ? : json_object();
    struct sqlite_cursor cursor;
    bool loop = true;
    json_t *map;

    if (!(sqlite_cursor_setup(sqlite, &cursor) &&
          sqlite_setup_query(&cursor, query)))
        return NULL;

    map = json_object();
    while (loop) {
        uint64_t last_read;
        const char *id;
        json_t *subobj;

        if (!sqlite_cursor_step(&cursor))
            break;

        if (errno == 0)
            /* last/only element */
            loop = false;

        id = sqlite_cursor_get_string(&cursor);
        if (!id)
            continue;
        last_read = sqlite_cursor_get_uint64(&cursor);

        subobj = json_object();
        json_object_set_new(subobj, "last_read", json_integer(last_read));
        json_object_set_new(map, strdup(id), subobj);
    }

    if (json_object_size(map) == 0) {
        json_decref(map);
        return NULL;
    }

    json_object_set_new(info, "fsevents_source", map);
    return info;
}

static json_t *
backend_sync_info(struct sqlite_backend *sqlite, json_t *previous_info,
                  bool first)
{
    json_t *info = previous_info ? : json_object();
    const char *query =
        "select "
        "    mountpoint, cli, duration, inserted, skipped, start, total, end "
        " from log order by start %s limit 1";
    const char *key = first ? "first_sync" : "last_sync";
    const char *mountpoint = NULL;
    struct sqlite_cursor cursor;
    const char *cli = NULL;
    int64_t duration = -1;
    int64_t inserted = -1;
    int64_t skipped = -1;
    int64_t start = -1;
    json_t *first_sync;
    int64_t total = -1;
    char *full_query;
    int64_t end = -1;

    if (asprintf(&full_query, query, first ? "ASC" : "DESC") < 0)
        goto err_info;

    if (!sqlite_cursor_setup(sqlite, &cursor))
        goto err_full_query;

    if (!sqlite_setup_query(&cursor, full_query))
        goto err_cursor;

    errno = 0;
    if (!sqlite_cursor_step(&cursor)) {
        if (errno != 0)
            goto err_cursor;

        /* data not present in the table, return NULL value */
        free(full_query);
        sqlite_cursor_fini(&cursor);
        json_object_set_new(info, key, json_null());
        return info;
    }

    mountpoint = sqlite_cursor_get_string(&cursor);
    cli = sqlite_cursor_get_string(&cursor);
    duration = sqlite_cursor_get_int64(&cursor);
    inserted = sqlite_cursor_get_int64(&cursor);
    skipped = sqlite_cursor_get_int64(&cursor);
    start = sqlite_cursor_get_int64(&cursor);
    total = sqlite_cursor_get_int64(&cursor);
    end = sqlite_cursor_get_int64(&cursor);

    first_sync = json_object();
    json_object_set_new(first_sync, "sync_debut", json_integer(start));
    json_object_set_new(first_sync, "sync_duration", json_integer(duration));
    json_object_set_new(first_sync, "sync_end", json_integer(end));
    json_object_set_new(first_sync, "mountpoint", json_string(mountpoint));
    json_object_set_new(first_sync, "command_line", json_string(cli));
    json_object_set_new(first_sync, "converted_entries",
                        json_integer(inserted));
    json_object_set_new(first_sync, "skipped_entries", json_integer(skipped));
    json_object_set_new(first_sync, "total_entries_seen", json_integer(total));
    json_object_set_new(info, key, first_sync);

    free(full_query);
    sqlite_cursor_fini(&cursor);

    return info;

err_cursor:
    sqlite_cursor_fini(&cursor);
err_full_query:
    free(full_query);
err_info:
    json_decref(info);
    return NULL;
}

static json_t *
backend_mountpoint(struct sqlite_backend *sqlite, json_t *previous_info)
{
    const char *query = "select mountpoint from info";
    json_t *info = previous_info ? : json_object();
    const char *mountpoint = NULL;
    struct sqlite_cursor cursor;

    if (!(sqlite_cursor_setup(sqlite, &cursor) &&
          sqlite_setup_query(&cursor, query)))
        return NULL;

    if (!sqlite_cursor_step(&cursor))
        return NULL;

    mountpoint = sqlite_cursor_get_string(&cursor);
    json_object_set_new(info, "mountpoint", json_string(mountpoint));

    sqlite_cursor_fini(&cursor);

    return info;
}

struct rbh_value_map *
sqlite_backend_get_info(void *backend, int flags)
{
    struct sqlite_backend *sqlite = backend;
    struct rbh_value_map *map;
    json_t *info = NULL;

    map = malloc(sizeof(*map));
    if (!map)
        return NULL;

    if (flags & RBH_INFO_BACKEND_SOURCE)
        info = backend_source(sqlite);
    if (flags & RBH_INFO_SIZE)
        info = backend_size(sqlite, info);
    if (flags & RBH_INFO_COUNT)
        info = backend_count(sqlite, info);
    if (flags & RBH_INFO_FSEVENTS_SOURCE)
        info = backend_fsevents_source(sqlite, info);
    if (flags & RBH_INFO_FIRST_SYNC)
        info = backend_sync_info(sqlite, info, true);
    if (flags & RBH_INFO_LAST_SYNC)
        info = backend_sync_info(sqlite, info, false);
    if (flags & RBH_INFO_MOUNTPOINT)
        info = backend_mountpoint(sqlite, info);

    if (!info)
        return NULL;

    if (!json2value_map(info, map, sqlite->sstack))
        goto free_map;

    json_decref(info);
    return map;

free_map:
    free(map);
    return NULL;
}
