/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood/utils.h>

#include "info.h"

#define WIDTH 32

struct rbh_info_fields {
    enum rbh_info flag;
    const char *helper;
    const char *header;
    const char *field;
    void (*value_function)(const struct rbh_value *, const char *);
};

static void
_get_size_value(const struct rbh_value *value, const char *header)
{
    char _buffer[32];
    char *buffer;

    buffer = _buffer;

    if (value->type == RBH_VT_INT32)
        size_printer(buffer, sizeof(_buffer), value->int32);
    else if (value->type == RBH_VT_INT64)
        size_printer(buffer, sizeof(_buffer), value->int64);

    printf("%s: %s\n", header, buffer);
}

static void
_get_int_value(const struct rbh_value *value, const char *header)
{
    if (value->type == RBH_VT_INT32)
        printf("%s: %d\n", header, value->int32);
    else if (value->type == RBH_VT_INT64)
        printf("%s: %ld\n", header, value->int64);
}

static void
print_conn_param(const struct rbh_value_map *map)
{
    printf("   Connection parameters used for this plugin:\n");
    for (int i = 0; i < map->count; i++) {
        const struct rbh_value_pair *pair = &map->pairs[i];

        assert(pair->value->type == RBH_VT_STRING);
        printf("   - %s: %s\n", pair->key, pair->value->string);
    }
}

static void
_get_backend_source(const struct rbh_value *value, const char *header)
{
    assert(value->type == RBH_VT_SEQUENCE);

    if (value->sequence.count == 0) {
        printf("%s: no source backend found\n", header);
        return;
    }

    printf("%s:\n", header);

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

        for (j = 0; j < submap->count; j++) {
            if (is_plugin && strcmp(submap->pairs[j].key, "plugin") == 0)
                printf(" - Plugin: %s\n", submap->pairs[j].value->string);

            if (!is_plugin && strcmp(submap->pairs[j].key, "extension") == 0)
                printf("   - Extension: %s\n", submap->pairs[j].value->string);

            if (is_plugin && strcmp(submap->pairs[j].key, "param") == 0)
                print_conn_param(&submap->pairs[j].value->map);
        }
    }
}

static void
_get_collection_sync(const struct rbh_value *value, const char *header)
{
    const struct rbh_value_map metadata_map = value->map;
    time_t time;

    assert(value->type == RBH_VT_MAP);

    printf("%s:\n", header);

    for (size_t i = 0 ; i < metadata_map.count ; i++) {
        const struct rbh_value_pair *pair = &metadata_map.pairs[i];
        if (strcmp(pair->key, "sync_debut") == 0) {
            time = (time_t)pair->value->int64;
            printf(" - %-*s %s\n", WIDTH, "Start of the sync:",
                   time_from_timestamp(&time));
        }

        if (strcmp(pair->key, "sync_duration") == 0) {
            char _buffer[32];
            size_t bufsize;
            char *buffer;

            buffer = _buffer;
            bufsize = sizeof(_buffer);

            difftime_printer(buffer, bufsize, pair->value->int64);

            printf(" - %-*s %s\n", WIDTH, "Duration of the sync:",
                   buffer);
        }

        if (strcmp(pair->key, "sync_end") == 0) {
            time = (time_t)pair->value->int64;
            printf(" - %-*s %s\n", WIDTH, "End of the sync:",
                   time_from_timestamp(&time));
        }

        if (strcmp(pair->key, "mountpoint") == 0)
            printf(" - %-*s %s\n", WIDTH, "Mountpoint used for the sync:",
                   pair->value->string);

        if (strcmp(pair->key, "command_line") == 0)
            printf(" - %-*s %s\n", WIDTH, "Command used for the sync:",
                   pair->value->string);

        if (strcmp(pair->key, "converted_entries") == 0)
            printf(" - %-*s %ld\n", WIDTH, "Amount of entries converted:",
                   pair->value->int64);

        if (strcmp(pair->key, "skipped_entries") == 0)
            printf(" - %-*s %ld\n", WIDTH, "Amount of entries skipped:",
                   pair->value->int64);

        if (strcmp(pair->key, "total_entries_seen") == 0)
            printf(" - %-*s %ld\n", WIDTH,
                   "Total entries seen by the sync:",
                   pair->value->int64);
    }
}

static void
_get_string_value(const struct rbh_value *value, const char *header)
{
    printf("%s: %s\n", header, value->string);
}

static void
_get_last_sync_start_date(const struct rbh_value *value, const char *header)
{
    time_t time = (time_t) value->int64;;

    printf("%s: %s (%ld)\n", header, time_from_timestamp(&time), time);
}

static struct rbh_info_fields INFO_FIELDS[] = {
    {
        RBH_INFO_AVG_OBJ_SIZE,
        "-a: print the average size of the entries in the mirror",
        "Average object size",
        "average_object_size",
        _get_size_value
    },
    {
        RBH_INFO_BACKEND_SOURCE,
        "-b: print the backend sources of the mirror",
        "Source backends",
        "backend_source",
        _get_backend_source
    },
    {
        RBH_INFO_COMMAND_BACKEND,
        "-B: print the backend specified in the last rbh-sync",
        "Last backend",
        "command_backend",
        _get_string_value
    },
    {
        RBH_INFO_COUNT,
        "-c: print the amount of entries inside the mirror",
        "Mirror entry count",
        "count",
        _get_int_value
    },
    {
        RBH_INFO_FIRST_SYNC,
        "-f: print info about the first rbh-sync done",
        "First sync",
        "first_sync",
        _get_collection_sync
    },
    {
        RBH_INFO_LAST_SYNC,
        "-l: print info about the last rbh-sync done",
        "Last sync",
        "last_sync",
        _get_collection_sync
    },
    {
        RBH_INFO_LAST_SYNC_START_DATE,
        "-d: print the starting date of the last rbh-sync done",
        "Last sync start date",
        "last_sync_start_date",
        _get_last_sync_start_date
    },
    {
        RBH_INFO_MOUNTPOINT,
        "-m: print info about the mountpoint of the last rbh-sync",
        "Last mountpoint",
        "mountpoint",
        _get_string_value
    },
    {
        RBH_INFO_SIZE,
        "-s: print the size of the mirror backend",
        "Mirror backend size",
        "size",
        _get_size_value
    },
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
            if (strcmp(pair->key, INFO_FIELDS[j].field) == 0)
                INFO_FIELDS[j].value_function(pair->value,
                                              INFO_FIELDS[j].header);
    }

    return 0;
}

void
info_translate(const struct rbh_backend_plugin *plugin)
{
    size_t field_count = sizeof(INFO_FIELDS) / sizeof(INFO_FIELDS[0]);
    const uint8_t info = plugin->info;

    if (!info) {
        printf("Currently no info available for plugin '%s'\n",
               plugin->plugin.name);
        return;
    }

    printf("Available info for plugin '%s': \n", plugin->plugin.name);
    for (size_t i = 0 ; i < field_count ; i++)
        if (info & INFO_FIELDS[i].flag)
            printf("%s\n", INFO_FIELDS[i].helper);
}
