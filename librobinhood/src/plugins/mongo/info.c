/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include "value.h"

#include "mongo.h"

#define MIN_VALUES_SSTACK_ALLOC (1 << 6)
static __thread struct rbh_sstack *info_sstack;

static void __attribute__((destructor))
destroy_sstack(void)
{
    if (info_sstack)
        rbh_sstack_destroy(info_sstack);
}

static int
get_collection_info(const struct mongo_backend *mongo, char *field_to_find,
                    struct rbh_value_pair *pair)
{
    struct rbh_value *value;
    mongoc_cursor_t *cursor;
    bson_t *filter = NULL;
    char _buffer[4096];
    bool found = false;
    const bson_t *doc;
    bson_iter_t iter;
    size_t bufsize;
    char *buffer;
    int rc = 0;

    buffer = _buffer;
    bufsize = sizeof(_buffer);
    value = RBH_SSTACK_PUSH(info_sstack, NULL, sizeof(*value));

    if (strcmp(field_to_find, "mountpoint") == 0)
        filter = BCON_NEW("_id", "mountpoint_info");
    if (strcmp(field_to_find, "backend_source") == 0 ||
        strcmp(field_to_find, "fsevents_source") == 0 ||
        strcmp(field_to_find, "command_backend") == 0)
        filter = BCON_NEW("_id", "backend_info");

    cursor = mongoc_collection_find_with_opts(mongo->info, filter, NULL, NULL);
    if (!mongoc_cursor_next(cursor, &doc)) {
        rc = -1;
        goto out;
    }

    if (!bson_iter_init(&iter, doc)) {
        rc = -1;
        goto out;
    }

    while (bson_iter_next(&iter)) {
        const char *key = bson_iter_key(&iter);

        if (strcmp(key, field_to_find) == 0) {
            if (!bson_iter_rbh_value(&iter, value, &buffer, &bufsize)) {
                rc = -1;
                goto out;
            }

            found = true;
            pair->key = field_to_find;
            pair->value = value_clone(value);
            break;
        }
    }

    if (!found)
        rc = -1;
out:
    if (cursor)
        mongoc_cursor_destroy(cursor);
    if (filter)
        bson_destroy(filter);

    return rc;
}

static int
get_collection_count(const struct mongo_backend *mongo,
                     struct rbh_value_pair *pair)
{
    bson_t *filter = bson_new();
    bson_t *opts = bson_new();
    struct rbh_value *value;
    bson_error_t error;
    int64_t count;

    value = RBH_SSTACK_PUSH(info_sstack, NULL, sizeof(*value));

    count = mongoc_collection_count_documents(mongo->entries, filter, opts,
                                              NULL, NULL, &error);
    if (count < 0)
        goto out;

    value->type = RBH_VT_INT64;
    value->int64 = count;

    pair->key = "count";
    pair->value = value;

out:
    bson_destroy(filter);
    bson_destroy(opts);

    if (count < 0) {
        fprintf(stderr, "Error while counting\n");
        return 0;
    }

    return 1;
}

static int
get_collection_stats(const struct mongo_backend *mongo, char *stats_to_find,
                     struct rbh_value_pair *pair)
{
    struct rbh_value *value;
    bson_error_t error;
    bson_iter_t iter;
    int64_t size = 0;
    bson_t *command;
    bson_t reply;

    value = RBH_SSTACK_PUSH(info_sstack, NULL, sizeof(*value));

    command = BCON_NEW("collStats", BCON_UTF8("entries"));

    if (!mongoc_collection_command_simple(mongo->entries, command, NULL,
                                          &reply, &error))
        goto out;

    if (bson_iter_init(&iter, &reply) &&
        bson_iter_find(&iter, stats_to_find)) {
        if (BSON_ITER_HOLDS_INT32(&iter))
            size = bson_iter_int32(&iter);
        else
            goto out;

    } else {
        goto out;
    }

    value->type = RBH_VT_INT64;
    value->int64 = size;

    if (strcmp(stats_to_find, "avgObjSize") == 0)
        pair->key = "average_object_size";
    else
        pair->key = stats_to_find;
    pair->value = value;

out:
    if (command)
        bson_destroy(command);
    bson_destroy(&reply);

    if (!size) {
        fprintf(stderr, "%s not avalaible\n", stats_to_find);
        return 0;
    }

    return 1;
}

static int
get_collection_sync(const struct mongo_backend *mongo, char *field_to_find,
                    struct rbh_value_pair *pair)
{
    struct rbh_value *value;
    mongoc_cursor_t *cursor;
    bson_t *opts = NULL;
    char _buffer[4096];
    const bson_t *doc;
    bson_iter_t iter;
    bson_t *filter;
    size_t bufsize;
    char *buffer;
    int rc = 0;

    buffer = _buffer;
    filter = bson_new();
    bufsize = sizeof(_buffer);
    value = RBH_SSTACK_PUSH(info_sstack, NULL, sizeof(*value));

    if (strcmp(field_to_find, "first_sync") == 0)
        opts = BCON_NEW("sort", "{", "_id", BCON_INT32(1), "}");

    if (strcmp(field_to_find, "last_sync") == 0)
        opts = BCON_NEW("sort", "{", "_id", BCON_INT32(-1), "}");

    cursor = mongoc_collection_find_with_opts(mongo->log, filter, opts, NULL);
    if (!cursor) {
        rc = -1;
        goto out;
    }

    if (!mongoc_cursor_next(cursor, &doc)) {
        rc = 1;
        goto out;
    }

    if (!bson_iter_init(&iter, doc)) {
        rc = 1;
        goto out;
    }

    while (bson_iter_next(&iter)) {
        const char *key = bson_iter_key(&iter);

        if (strcmp(key, "sync_metadata") == 0) {
            if (!bson_iter_rbh_value(&iter, value, &buffer, &bufsize)) {
                rc = 1;
                goto out;
            }

            pair->key = field_to_find;
            pair->value = value_clone(value);
        }
    }

out:
    if (cursor)
        mongoc_cursor_destroy(cursor);
    if (filter)
        bson_destroy(filter);
    if (opts)
        bson_destroy(opts);

    return rc;
}

static int
get_collection_last_sync_start_date(const struct mongo_backend *mongo,
                                    struct rbh_value_pair *pair)
{
    const struct rbh_value *sync_debut_value;
    struct rbh_value *value;
    mongoc_cursor_t *cursor;
    bson_t *opts = NULL;
    char _buffer[4096];
    const bson_t *doc;
    bson_iter_t iter;
    bson_t *filter;
    size_t bufsize;
    char *buffer;
    int rc = 0;

    buffer = _buffer;
    filter = bson_new();
    bufsize = sizeof(_buffer);
    value = RBH_SSTACK_PUSH(info_sstack, NULL, sizeof(*value));
    opts = BCON_NEW("sort", "{", "_id", BCON_INT32(-1), "}");

    cursor = mongoc_collection_find_with_opts(mongo->log, filter, opts, NULL);
    if (!cursor) {
        rc = -1;
        goto out;
    }

    if (!mongoc_cursor_next(cursor, &doc)) {
        rc = 1;
        goto out;
    }

    if (!bson_iter_init(&iter, doc)) {
        rc = 1;
        goto out;
    }

    while (bson_iter_next(&iter)) {
        const char *key = bson_iter_key(&iter);

        if (strcmp(key, "sync_metadata") == 0) {
            if (!bson_iter_rbh_value(&iter, value, &buffer, &bufsize)) {
                rc = 1;
                goto out;
            }

            assert(value->type == RBH_VT_MAP);
            sync_debut_value = rbh_map_find(&value->map, "sync_debut");
            assert(sync_debut_value);

            pair->key = "last_sync_start_date";
            pair->value = value_clone(sync_debut_value);
            break;
        }
    }

out:
    if (cursor)
        mongoc_cursor_destroy(cursor);
    if (filter)
        bson_destroy(filter);
    if (opts)
        bson_destroy(opts);

    return rc;
}

struct rbh_value_map *
mongo_backend_get_info(void *backend, int info_flags)
{
    struct mongo_backend *mongo = backend;
    struct rbh_value_map *map_value;
    struct rbh_value_pair *pairs;
    int tmp_flags = info_flags;
    int count = 0;
    int idx = 0;

    while (tmp_flags) {
        count += tmp_flags & 1;
        tmp_flags >>= 1;
    }

    if (info_sstack == NULL)
        info_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                    (sizeof(struct rbh_value_map *)));

    pairs = RBH_SSTACK_PUSH(info_sstack, NULL, count * sizeof(*pairs));
    map_value = RBH_SSTACK_PUSH(info_sstack, NULL, sizeof(*map_value));

    if (info_flags & RBH_INFO_AVG_OBJ_SIZE) {
        if (!get_collection_stats(mongo, "avgObjSize", &pairs[idx++]))
            goto out;
    }

    if (info_flags & RBH_INFO_BACKEND_SOURCE) {
        if (get_collection_info(mongo, "backend_source", &pairs[idx++]) == -1)
            goto out;
    }

    if (info_flags & RBH_INFO_COMMAND_BACKEND) {
        if (get_collection_info(mongo, "command_backend", &pairs[idx++]) == -1)
            goto out;
    }

    if (info_flags & RBH_INFO_COUNT) {
        if (!get_collection_count(mongo, &pairs[idx++]))
            goto out;
    }

    if (info_flags & RBH_INFO_FIRST_SYNC) {
        if (get_collection_sync(mongo, "first_sync", &pairs[idx++]))
            goto out;
    }

    if (info_flags & RBH_INFO_LAST_SYNC) {
        if (get_collection_sync(mongo, "last_sync", &pairs[idx++]))
            goto out;
    }

    if (info_flags & RBH_INFO_LAST_SYNC_START_DATE) {
        if (get_collection_last_sync_start_date(mongo, &pairs[idx++]))
            goto out;
    }

    if (info_flags & RBH_INFO_MOUNTPOINT) {
        if (get_collection_info(mongo, "mountpoint", &pairs[idx++]))
            goto out;
    }

    if (info_flags & RBH_INFO_SIZE) {
        if (!get_collection_stats(mongo, "size", &pairs[idx++]))
            goto out;
    }

    if (info_flags & RBH_INFO_FSEVENTS_SOURCE) {
        if (get_collection_info(mongo, "fsevents_source", &pairs[idx++]))
            goto out;
    }

    map_value->pairs = pairs;
    map_value->count = idx;

    return map_value;

out:
    errno = EINVAL;
    return NULL;
}
