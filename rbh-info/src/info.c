/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood/utils.h>

#include "info.h"

#define WIDTH 32

struct rbh_info_fields {
    char *field_name;
    void (*value_function)(const struct rbh_value *value);
};

static void
_get_avg_obj_size(const struct rbh_value *value)
{
    char _buffer[32];
    char *buffer;

    buffer = _buffer;

    size_printer(buffer, sizeof(_buffer), value->int32);

    printf("%s\n", buffer);
}

static void
_get_backend_source(const struct rbh_value *value)
{
    assert(value->type == RBH_VT_SEQUENCE);

    for (size_t i = 0; i < value->sequence.count; i++) {
        const struct rbh_value_map *submap = &value->sequence.values[i].map;
        size_t type_index = -1;
        bool is_plugin;
        size_t j;

        for (j = 0; j < submap->count; j++) {
            if (strcmp(submap->pairs[j].key, "type") == 0) {
                type_index = j;
                break;
            }
        }

        assert(j != -1);
        assert(submap->pairs[type_index].value->type == RBH_VT_STRING);
        is_plugin = (strcmp(submap->pairs[type_index].value->string,
                            "plugin") == 0);

        for (j = 0; j < submap->count; j++)
            if ((is_plugin && strcmp(submap->pairs[j].key, "plugin") == 0) ||
                (!is_plugin && strcmp(submap->pairs[j].key, "extension") == 0))
                printf("%s\n", submap->pairs[j].value->string);
    }
}

static void
_get_count(const struct rbh_value *value)
{
    printf("%ld\n", value->int64);
}

static void
_get_sync(const struct rbh_value *value)
{
    assert(value->type == RBH_VT_MAP);

    const struct rbh_value_map metadata_map = value->map;
    time_t time;

    for (size_t i = 0 ; i < metadata_map.count ; i++) {
        const struct rbh_value_pair *pair = &metadata_map.pairs[i];
        if (strcmp(pair->key, "sync_debut") == 0) {
            time = (time_t)pair->value->int64;
            printf("%-*s %s\n", WIDTH, "Start of the sync:",
                   time_from_timestamp(&time));
        }

        if (strcmp(pair->key, "sync_duration") == 0) {
            char _buffer[32];
            size_t bufsize;
            char *buffer;

            buffer = _buffer;
            bufsize = sizeof(_buffer);

            difftime_printer(buffer, bufsize, pair->value->int64);

            printf("%-*s %s\n", WIDTH, "Duration of the sync:",
                   buffer);
        }

        if (strcmp(pair->key, "sync_end") == 0) {
            time = (time_t)pair->value->int64;
            printf("%-*s %s\n", WIDTH, "End of the sync:",
                   time_from_timestamp(&time));
        }

        if (strcmp(pair->key, "mountpoint") == 0)
            printf("%-*s %s\n", WIDTH, "Mountpoint used for the sync:",
                   pair->value->string);

        if (strcmp(pair->key, "command_line") == 0)
            printf("%-*s %s\n", WIDTH, "Command used for the sync:",
                   pair->value->string);

        if (strcmp(pair->key, "converted_entries") == 0)
            printf("%-*s %ld\n", WIDTH, "Amount of entries converted:",
                   pair->value->int64);

        if (strcmp(pair->key, "skipped_entries") == 0)
            printf("%-*s %ld\n", WIDTH, "Amount of entries skipped:",
                   pair->value->int64);

        if (strcmp(pair->key, "total_entries_seen") == 0)
            printf("%-*s %ld\n", WIDTH,
                   "Total entries seen by the sync:",
                   pair->value->int64);
    }
}

static void
_get_size(const struct rbh_value *value)
{
    char _buffer[32];
    char *buffer;

    buffer = _buffer;

    size_printer(buffer, sizeof(_buffer), value->int32);

    printf("%s\n", buffer);
}

static struct rbh_info_fields INFO_FIELDS[] = {
    { "average_object_size", _get_avg_obj_size },
    { "backend_source", _get_backend_source},
    { "count", _get_count },
    { "first_sync", _get_sync },
    { "last_sync", _get_sync },
    { "size", _get_size },
};

int
print_info_fields(struct rbh_backend *from, int flags)
{
    struct rbh_value_map *info_map = rbh_backend_get_info(from, flags);
    size_t field_count = sizeof(INFO_FIELDS) / sizeof(INFO_FIELDS[0]);

    if (info_map == NULL) {
        printf("Failed to retrieve requested information\n");
        return 1;
    }

    for (size_t i = 0 ; i < info_map->count ; i++) {
        const struct rbh_value_pair *pair = &info_map->pairs[i];

        for (size_t j = 0 ; j < field_count ; j++)
            if (strcmp(pair->key, INFO_FIELDS[j].field_name) == 0)
                INFO_FIELDS[j].value_function(pair->value);
    }

    return 0;
}

void
info_translate(const struct rbh_backend_plugin *plugin)
{
    const uint8_t info = plugin->info;

    if (!info) {
        printf("Currently no info available for plugin '%s'\n",
               plugin->plugin.name);
        return;
    }

    printf("Available info for plugin '%s': \n", plugin->plugin.name);
    if (info & RBH_INFO_AVG_OBJ_SIZE)
        printf("- a: give the average size of objects inside entries collection\n");

    if (info & RBH_INFO_BACKEND_SOURCE)
        printf("- b: give the backend sources of the backend\n");

    if (info & RBH_INFO_COUNT)
        printf("- c: retrieve the amount of document inside entries collection\n");

    if (info & RBH_INFO_SIZE)
        printf("- s: size of entries collection\n");

    if (info & RBH_INFO_FIRST_SYNC)
        printf("- f: info about the first rbh-sync done\n");

    if (info & RBH_INFO_LAST_SYNC)
        printf("- y: info about the last rbh-sync done\n");
}
