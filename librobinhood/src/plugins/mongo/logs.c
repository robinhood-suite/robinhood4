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

static int64_t
get_current_id(struct mongo_backend *mongo)
{
    mongoc_find_and_modify_opts_t *opts;
    bson_iter_t child_iter;
    bson_t *filter = NULL;
    bson_error_t error;
    bson_iter_t iter;
    char *error_str;
    bson_t *update;
    bson_t reply;
    bool success;
    int rc;

    /* Find and update the log_id atomically */
    filter = BCON_NEW("_id", "log_id");
    update = BCON_NEW("$inc", "{", "log_id", BCON_INT32(1), "}");

    opts = mongoc_find_and_modify_opts_new();
    mongoc_find_and_modify_opts_set_update(opts, update);
    /* Create the document if it didn't exist, and return the updated document */
    mongoc_find_and_modify_opts_set_flags(opts,
                                          MONGOC_FIND_AND_MODIFY_UPSERT |
                                          MONGOC_FIND_AND_MODIFY_RETURN_NEW);

    success = mongoc_collection_find_and_modify_with_opts(mongo->info, filter,
                                                          opts, &reply, &error);
    if (!success) {
        fprintf(stderr, "Failed to retrieve log id: %s\n", error.message);
        rc = -1;
        goto out;
    }

    /* Convoluted, but that's bson for you... 'reply' looks like this:
     * { "lastErrorObject" :
     *      { "n" : { "$numberInt" : "1" },
     *        "updatedExisting" : true
     *      },
     *   "value" :
     *      { "_id" : "log_id",
     *        "log_id" : { "$numberInt" : "3" }
     *      },
     *   "ok" : { "$numberDouble" : "1.0" }
     * }
     * So we have to find the "value" document which contains the "log_id" value
     * we updated and convert it.
     */
    if (!bson_iter_init(&iter, &reply) || !bson_iter_find(&iter, "value") ||
        !bson_iter_recurse(&iter, &child_iter) || !bson_iter_find(&child_iter,
                                                                  "log_id")) {
        error_str = bson_as_canonical_extended_json(&reply, NULL);
        fprintf(stderr, "Failed to find log id in iterator: %s\n", error_str);
        bson_free(error_str);
        rc = -1;
        goto out;
    }

    rc = bson_iter_as_int64(&child_iter);

out:
    bson_destroy(&reply);
    bson_destroy(update);
    mongoc_find_and_modify_opts_destroy(opts);
    bson_destroy(filter);

    return rc;
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
    int64_t log_id;
    int result;
    int rc = 0;

    log_id = get_current_id(mongo);
    if (log_id < 0) {
        fprintf(stderr, "Failed to retrieve log id to insert new log\n");
        rc = -1;
        goto skip_insert;
    }

    collection = mongo->log;
    update = bson_new();

    filter = BCON_NEW("_id", BCON_INT64(log_id));
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
get_logs(const struct mongo_backend *mongo, struct rbh_value_pair *pair,
         struct rbh_log_options *options)
{
    const char *str_type = rbh_log_type2str(options->type);
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
    bufsize = sizeof(_buffer);

    if (options->type == RBH_ALL_LOG)
        filter = bson_new();
    else
        filter = BCON_NEW(str_type, "{", "$exists", "true", "}");

    opts = BCON_NEW("limit", BCON_INT64(options->count),
                    "sort", "{", "_id", BCON_INT32(-1), "}");

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

            /* If we request a specific log type, check the given key is of that
             * type. If we don't request a specific log type, check the key
             * corresponds to a known log type.
             */
            if ((options->type != RBH_ALL_LOG && strcmp(key, str_type) == 0) ||
                (options->type == RBH_ALL_LOG &&
                    str2rbh_log_type(key) != RBH_ALL_LOG)) {
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
