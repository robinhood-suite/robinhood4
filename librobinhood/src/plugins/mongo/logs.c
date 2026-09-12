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
#include <unistd.h>

#include "robinhood/utils.h"
#include "value.h"

#include "mongo.h"

#define MIN_VALUES_SSTACK_ALLOC (1 << 12)
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
    bson_t *update = NULL;
    bson_error_t error;
    struct timeval now;
    int result;
    int rc = 0;

    gettimeofday(&now, NULL);
    collection = mongo->log;
    update = bson_new();

    if (!(BSON_APPEND_DATE_TIME(
            update, "logged_at",
            (int64_t) (now.tv_sec * 1000 + now.tv_usec / 1000)
          ) && BSON_APPEND_RBH_VALUE_MAP(update, command, map))) {
        fprintf(stderr, "Error while appending rbh_value to bson\n");
        rc = -1;
        goto skip_insert;
    }

    result = mongoc_collection_insert_one(collection, update, NULL,
                                          NULL, &error);
    if (!result) {
        fprintf(stderr, "Log insertion failed: %s\n", error.message);
        rc = -1;
    }

skip_insert:
    if (update)
        bson_destroy(update);

    return rc;
}

static int
log_type_to_bson_t(bson_t *filter, size_t types)
{
    int type = RBH_LOG_TYPE_FIRST;
    int counter = 0;
    bson_t array;

    if (!BSON_APPEND_ARRAY_BEGIN(filter, "$or", &array))
        return 1;

    while (type <= RBH_LOG_TYPE_LAST) {
        const char *str_type;
        bson_t subdocument;
        size_t key_length;
        bson_t document;
        const char *key;
        char str[16];

        if (!(type & types)) {
            type = type << 1;
            continue;
        }

        str_type = rbh_log_type2str(type);
        key_length = bson_uint32_to_string(counter, &key, str, sizeof(str));

        if (!(bson_append_document_begin(&array, key, key_length, &document) &&
              BSON_APPEND_DOCUMENT_BEGIN(&document, str_type, &subdocument) &&
              BSON_APPEND_BOOL(&subdocument, "$exists", true) &&
              bson_append_document_end(&document, &subdocument) &&
              bson_append_document_end(&array, &document)))
            return 1;

        type = type << 1;
        counter++;
    }

    return !bson_append_array_end(filter, &array);
}

static int
get_logs(const struct mongo_backend *mongo, struct rbh_value_pair *pair,
         struct rbh_log_options *options)
{
    mongoc_cursor_t *cursor = NULL;
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

    filter = bson_new();
    if (options->type != RBH_ALL_LOG &&
        log_type_to_bson_t(filter, options->type)) {
        rc = 1;
        goto out;
    }

    opts = BCON_NEW("limit", BCON_INT64(options->count),
                    "sort", "{",
                                "logged_at",
                                    BCON_INT32(options->ascending ? 1 : -1),
                                "_id",
                                    BCON_INT32(options->ascending ? 1 : -1),
                            "}");

    cursor = mongoc_collection_find_with_opts(mongo->log, filter, opts, NULL);
    if (!cursor) {
        rc = 1;
        goto out;
    }

    for (index = 0; index < options->count; ++index) {
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

            buffer = _buffer;
            bufsize = sizeof(_buffer);

            /* If we request a specific log type, check the given key is of that
             * type. If we don't request a specific log type, check the key
             * corresponds to a known log type.
             */
            if (str2rbh_log_type(key) != RBH_ALL_LOG) {
                if (!bson_iter_rbh_value(&iter, &value, &buffer, &bufsize)) {
                    rc = 1;
                    goto out;
                }

                pair[index].key = RBH_SSTACK_PUSH(logs_sstack, key,
                                                  strlen(key) + 1);
                pair[index].value = value_clone(&value);
            }
        }
    }

    options->count = index;

out:
    if (cursor)
        mongoc_cursor_destroy(cursor);
    if (filter)
        bson_destroy(filter);
    if (opts)
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

    options.count = (options.count > INT64_MAX ? INT64_MAX : options.count);

    if (logs_sstack == NULL)
        logs_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                    (sizeof(struct rbh_value_map *)));

    /* XXX: since we might request more logs than available, there will be more
     * pairs allocated than necessary. But since this is a one-shot command, no
     * need to care for that.
     */
    pairs = RBH_SSTACK_PUSH(logs_sstack, NULL, options.count * sizeof(*pairs));
    map_value = RBH_SSTACK_PUSH(logs_sstack, NULL, sizeof(*map_value));

    if (get_logs(mongo, pairs, &options))
        goto out;

    map_value->pairs = pairs;
    map_value->count = options.count;

    return map_value;

out:
    errno = EINVAL;
    return NULL;
}

struct rbh_value_map *
mongo_backend_get_log_count(void *backend)
{
    struct mongo_backend *mongo = backend;
    struct rbh_value_map *map_value;
    struct rbh_value_pair *pairs;
    bson_t *opts = bson_new();
    struct rbh_value *values;
    bson_error_t error;
    int index = 0;

    if (logs_sstack == NULL)
        logs_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                     (sizeof(struct rbh_value_map *)));

    values = RBH_SSTACK_PUSH(logs_sstack, NULL,
                             RBH_LOG_TYPE_COUNT * sizeof(*values));
    pairs = RBH_SSTACK_PUSH(logs_sstack, NULL,
                            RBH_LOG_TYPE_COUNT * sizeof(*pairs));
    map_value = RBH_SSTACK_PUSH(logs_sstack, NULL, sizeof(*map_value));

    for (enum rbh_log_type type = RBH_LOG_TYPE_FIRST;
         type <= RBH_LOG_TYPE_LAST;
         type = type << 1) {
        const char *key = rbh_log_type2str(type);
        bson_t *filter;
        int64_t count;

        filter = BCON_NEW(key, "{", "$exists", BCON_BOOL(true), "}");
        count = mongoc_collection_count_documents(mongo->log, filter, opts,
                                                  NULL, NULL, &error);
        bson_destroy(filter);
        if (count < 0) {
            map_value = NULL;
            goto out;
        }

        values[index].type = RBH_VT_INT64;
        values[index].int64 = count;

        pairs[index].key = RBH_SSTACK_PUSH(logs_sstack, key,
                                           strlen(key) + 1);
        pairs[index].value = &values[index];

        index++;
    }

    map_value->pairs = pairs;
    map_value->count = index;

out:
    bson_destroy(opts);

    return map_value;
}

int
mongo_backend_delete_logs(void *backend, struct rbh_log_options options)
{
    struct mongo_backend *mongo = backend;
    mongoc_cursor_t *cursor = NULL;
    const char **keys = NULL;
    bson_t *selector = NULL;
    bson_oid_t *ids = NULL;
    bson_t *opts = NULL;
    bson_error_t error;
    const bson_t *doc;
    bson_iter_t iter;
    bson_t *filter;
    int index = 0;
    bson_t array;
    char str[16];
    bool result;
    int rc = 0;

    filter = bson_new();
    if (options.type != RBH_ALL_LOG &&
        log_type_to_bson_t(filter, options.type)) {
        rc = -1;
        goto out;
    }

    opts = BCON_NEW("limit", BCON_INT64(options.count),
                    "projection", "{", "_id", BCON_BOOL(true), "}",
                    "sort", "{",
                                "logged_at",
                                    BCON_INT32(options.ascending ? 1 : -1),
                                "_id",
                                    BCON_INT32(options.ascending ? 1 : -1),
                            "}");

    ids = xcalloc(options.count, sizeof(*ids));

    /**
     * Mongo is very annoying here, there are three ways to delete entries in
     * Mongo:
     *  - deleteOne
     *  - deleteMany
     *  - findAndModify with deletion
     *
     * The first can only delete one entry with a specific filter, and cannot
     * sort, so we cannot use it to simply delete the oldest log over and over
     * again.
     * The second way has the same issue.
     * The third way can filter entries and sort them, but it can only delete
     * one entry at a time, meaning if we want to delete N logs, we have to comb
     * through all logs N time, and delete one each time,
     * i.e. N find + deleteOne...
     *
     * We chose to use the second way + a find on all the logs for the requested
     * filter. We first do the find and store the ID of all the logs to find,
     * then we request a deleteMany with a filter to match all logs with an ID
     * in the array of stored IDs.
     */
    cursor = mongoc_collection_find_with_opts(mongo->log, filter, opts, NULL);
    bson_destroy(filter);
    filter = NULL;
    bson_destroy(opts);
    opts = NULL;
    if (!cursor) {
        rc = 1;
        goto out;
    }

    for (index = 0; index < options.count; ++index) {
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
            assert(BSON_ITER_HOLDS_OID(&iter));
            bson_oid_copy(bson_iter_oid(&iter), &ids[index]);
        }
    }

    selector = bson_new();
    bson_t document;

    if (!(BSON_APPEND_DOCUMENT_BEGIN(selector, "_id", &document) &&
          BSON_APPEND_ARRAY_BEGIN(&document, "$in", &array))) {
        rc = -1;
        goto out;
    }

    for (uint32_t i = 0; i < index; i++) {
        const char *key;
        int key_length;

        key_length = bson_uint32_to_string(i, &key, str, sizeof(str));
        if (!bson_append_oid(&array, key, key_length, &ids[i])) {
            rc = -1;
            goto out;
        }
    }

    if (!(bson_append_array_end(&document, &array) &&
          bson_append_document_end(selector, &document))) {
        rc = -1;
        goto out;
    }

    result = mongoc_collection_delete_many(mongo->log, selector, NULL, NULL,
                                           &error);
    if (!result) {
        fprintf(stderr, "Failed to delete logs: %s\n", error.message);
        rc = -1;
    }

out:
    free(keys);
    if (cursor)
        mongoc_cursor_destroy(cursor);
    if (selector)
        bson_destroy(selector);
    free(ids);
    if (filter)
        bson_destroy(filter);

    return rc;

handle_error:
    if (cursor)
        mongoc_cursor_destroy(cursor);

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
