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
    value = RBH_SSTACK_PUSH(logs_sstack, NULL, sizeof(*value));

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

struct rbh_value_map *
mongo_backend_get_logs(void *backend, struct rbh_log_options options)
{
    struct mongo_backend *mongo = backend;
    struct rbh_value_map *map_value;
    struct rbh_value_pair *pairs;
    int count = 1;
    int idx = 0;

    if (logs_sstack == NULL)
        logs_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                    (sizeof(struct rbh_value_map *)));

    pairs = RBH_SSTACK_PUSH(logs_sstack, NULL, count * sizeof(*pairs));
    map_value = RBH_SSTACK_PUSH(logs_sstack, NULL, sizeof(*map_value));

    if (options.ascending) {
        if (get_collection_sync(mongo, "first_sync", &pairs[idx++]))
            goto out;
    } else {
        if (get_collection_sync(mongo, "last_sync", &pairs[idx++]))
            goto out;
    }

    map_value->pairs = pairs;
    map_value->count = idx;

    return map_value;

out:
    errno = EINVAL;
    return NULL;
}
