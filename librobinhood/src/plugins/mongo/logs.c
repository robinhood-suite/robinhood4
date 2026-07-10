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
static __thread struct rbh_sstack *logs_sstack;

static void __attribute__((destructor))
destroy_sstack(void)
{
    if (logs_sstack)
        rbh_sstack_destroy(logs_sstack);
}

int
mongo_backend_insert_log(void *backend, const char *command,
                         const struct rbh_value_map *map)
{
    struct mongo_backend *mongo = backend;
    mongoc_collection_t *collection;
    bson_t *filter = NULL;
    bson_t *update = NULL;
    bson_t *opts = NULL;
    bson_t metadata_doc;
    bson_error_t error;
    int result;
    int rc = 0;

    collection = mongo->log;
    update = bson_new();

    filter = BCON_NEW("_id", BCON_INT64(time(NULL)));
    opts = BCON_NEW("upsert", BCON_BOOL(true));

    if (!(BSON_APPEND_DOCUMENT_BEGIN(update, "$set", &metadata_doc)
        && BSON_APPEND_RBH_VALUE_MAP(&metadata_doc, command, map)
        && bson_append_document_end(update, &metadata_doc))) {
        fprintf(stderr, "Error while appending rbh_value to bson\n");
        rc = -1;
        goto skip_insert;
    }

    result = mongoc_collection_update_one(collection, filter, update, opts,
                                          NULL, &error);
    if (!result) {
        fprintf(stderr, "Upsert failed: %s\n", error.message);
        rc = -1;
    }

skip_insert:
    if (filter)
        bson_destroy(filter);
    if (update)
        bson_destroy(update);
    if (opts)
        bson_destroy(opts);

    return rc;
}

static int
get_collection_sync(const struct mongo_backend *mongo, char *field_to_find,
                    struct rbh_value_pair *pair, size_t *count)
{
    mongoc_cursor_t *cursor;
    struct rbh_value value;
    bson_t *opts = NULL;
    bson_error_t error;
    char _buffer[4096];
    const bson_t *doc;
    bson_iter_t iter;
    bson_t *filter;
    size_t bufsize;
    int index = 0;
    char *buffer;
    int rc = 0;

    buffer = _buffer;
    filter = bson_new();
    bufsize = sizeof(_buffer);

    if (strcmp(field_to_find, "first_sync") == 0)
        opts = BCON_NEW("limit", BCON_INT64(*count),
                        "sort", "{", "_id", BCON_INT32(1), "}");

    if (strcmp(field_to_find, "last_sync") == 0)
        opts = BCON_NEW("limit", BCON_INT64(*count),
                        "sort", "{", "_id", BCON_INT32(-1), "}");

    cursor = mongoc_collection_find_with_opts(mongo->log, filter, opts, NULL);
    if (!cursor) {
        rc = 1;
        goto out;
    }

    for (index = 0; index < *count; ++index) {
        if (!mongoc_cursor_more(cursor)) {
            if (mongoc_cursor_error(cursor, &error)) {
                rc = 1;
                goto handle_error;
            }

            rc = 0;
            break;
        }

        if (!mongoc_cursor_next(cursor, &doc)) {
            if (mongoc_cursor_error(cursor, &error)) {
                rc = 1;
                goto handle_error;
            }

            rc = 0;
            break;
        }

        if (!bson_iter_init(&iter, doc)) {
            rc = 1;
            goto out;
        }

        while (bson_iter_next(&iter)) {
            const char *key = bson_iter_key(&iter);

            if (strcmp(key, "sync") == 0) {
                if (!bson_iter_rbh_value(&iter, &value, &buffer, &bufsize)) {
                    rc = 1;
                    goto out;
                }

                pair[index].key = field_to_find;
                pair[index].value = value_clone(&value);
            }
        }
    }

    *count = index;

out:
    if (cursor)
        mongoc_cursor_destroy(cursor);
    bson_destroy(filter);
    bson_destroy(opts);

    return rc;

handle_error:
    mongoc_cursor_destroy(cursor);
    bson_destroy(filter);
    bson_destroy(opts);

    switch (error.domain) {
    case MONGOC_ERROR_SERVER_SELECTION:
        switch (error.code) {
        case MONGOC_ERROR_SERVER_SELECTION_FAILURE:
            errno = ENOTCONN;
            return 1;
        }
        break;
    }
    snprintf(rbh_backend_error, sizeof(rbh_backend_error), "%d.%d: %s",
             error.domain, error.code, error.message);
    errno = RBH_BACKEND_ERROR;

    return rc;
}

struct rbh_value_map *
mongo_backend_get_logs(void *backend, struct rbh_log_options options)
{
    struct mongo_backend *mongo = backend;
    struct rbh_value_map *map_value;
    struct rbh_value_pair *pairs;
    size_t count;

    count = (options.count > INT64_MAX ? INT64_MAX : options.count);

    if (logs_sstack == NULL)
        logs_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                    (sizeof(struct rbh_value_map *)));

    /* XXX: since we might request more logs than available, there will be more
     * pairs allocated than necessary. But since this is a one-shot command, no
     * need to care for that.
     */
    pairs = RBH_SSTACK_PUSH(logs_sstack, NULL, count * sizeof(*pairs));
    map_value = RBH_SSTACK_PUSH(logs_sstack, NULL, sizeof(*map_value));

    if (options.ascending) {
        if (get_collection_sync(mongo, "first_sync", pairs, &count))
            goto out;
    } else {
        if (get_collection_sync(mongo, "last_sync", pairs, &count))
            goto out;
    }

    map_value->pairs = pairs;
    map_value->count = count;

    return map_value;

out:
    errno = EINVAL;
    return NULL;
}
