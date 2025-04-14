/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>

/* This backend uses libmongoc, from the "mongo-c-driver" project to interact
 * with a MongoDB database.
 *
 * The documentation for the project can be found at: https://mongoc.org
 */

#include <bson.h>
#include <mongoc.h>
#include <miniyaml.h>

#include "robinhood/backends/mongo.h"
#include "robinhood/itertools.h"
#include "robinhood/ringr.h"
#include "robinhood/statx.h"
#include "robinhood/sstack.h"
#include "utils.h"

#include "mongo.h"

/* libmongoc imposes that mongoc_init() be called before any other mongoc_*
 * function; and mongoc_cleanup() after the last one.
 */

__attribute__((constructor))
static void
mongo_init(void)
{
    mongoc_init();
}

__attribute__((destructor))
static void
mongo_cleanup(void)
{
    mongoc_cleanup();
}

#define MIN_VALUES_SSTACK_ALLOC (1 << 6)
struct rbh_sstack *values_sstack;
struct rbh_sstack *info_sstack;

static void __attribute__((destructor))
destroy_sstack(void)
{
    if (values_sstack)
        rbh_sstack_destroy(values_sstack);
    if (info_sstack)
        rbh_sstack_destroy(info_sstack);
}

    /*--------------------------------------------------------------------*
     |                       bson_pipeline_creation                       |
     *--------------------------------------------------------------------*/

static const char *UINT8_TO_STR[256] = {
     "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
     "8",  "9",  "10",  "11",  "12",  "13",  "14",  "15",
     "16",  "17",  "18",  "19",  "20",  "21",  "22",  "23",
     "24",  "25",  "26",  "27",  "28",  "29",  "30",  "31",
     "32",  "33",  "34",  "35",  "36",  "37",  "38",  "39",
     "40",  "41",  "42",  "43",  "44",  "45",  "46",  "47",
     "48",  "49",  "50",  "51",  "52",  "53",  "54",  "55",
     "56",  "57",  "58",  "59",  "60",  "61",  "62",  "63",
     "64",  "65",  "66",  "67",  "68",  "69",  "70",  "71",
     "72",  "73",  "74",  "75",  "76",  "77",  "78",  "79",
     "80",  "81",  "82",  "83",  "84",  "85",  "86",  "87",
     "88",  "89",  "90",  "91",  "92",  "93",  "94",  "95",
     "96",  "97",  "98",  "99", "100", "101", "102", "103",
    "104", "105", "106", "107", "108", "109", "110", "111",
    "112", "113", "114", "115", "116", "117", "118", "119",
    "120", "121", "122", "123", "124", "125", "126", "127",
    "128", "129", "130", "131", "132", "133", "134", "135",
    "136", "137", "138", "139", "140", "141", "142", "143",
    "144", "145", "146", "147", "148", "149", "150", "151",
    "152", "153", "154", "155", "156", "157", "158", "159",
    "160", "161", "162", "163", "164", "165", "166", "167",
    "168", "169", "170", "171", "172", "173", "174", "175",
    "176", "177", "178", "179", "180", "181", "182", "183",
    "184", "185", "186", "187", "188", "189", "190", "191",
    "192", "193", "194", "195", "196", "197", "198", "199",
    "200", "201", "202", "203", "204", "205", "206", "207",
    "208", "209", "210", "211", "212", "213", "214", "215",
    "216", "217", "218", "219", "220", "221", "222", "223",
    "224", "225", "226", "227", "228", "229", "230", "231",
    "232", "233", "234", "235", "236", "237", "238", "239",
    "240", "241", "242", "243", "244", "245", "246", "247",
    "248", "249", "250", "251", "252", "253", "254", "255"
};

static bson_t *
bson_pipeline_creation(const struct rbh_filter *filter,
                       const struct rbh_group_fields *group,
                       const struct rbh_filter_options *options,
                       const struct rbh_filter_output *output)
{
    bson_t *pipeline;
    uint8_t i = 0;
    bson_t array;
    bson_t stage;

    (void) group;
    (void) output;

    if (options->skip > INT64_MAX || options->limit > INT64_MAX) {
        errno = ENOTSUP;
        return NULL;
    }

    pipeline = bson_new();

    if (BSON_APPEND_ARRAY_BEGIN(pipeline, "pipeline", &array)
     && BSON_APPEND_DOCUMENT_BEGIN(&array, UINT8_TO_STR[i], &stage) && ++i
     && BSON_APPEND_UTF8(&stage, "$unwind", "$" MFF_NAMESPACE)
     && bson_append_document_end(&array, &stage)
     && BSON_APPEND_DOCUMENT_BEGIN(&array, UINT8_TO_STR[i], &stage) && ++i
     && BSON_APPEND_RBH_FILTER(&stage, "$match", filter)
     && bson_append_document_end(&array, &stage)
     && (group && is_set_for_range_needed(group) ?
            BSON_APPEND_DOCUMENT_BEGIN(&array, UINT8_TO_STR[i], &stage) && ++i
            && BSON_APPEND_AGGREGATE_SET_STAGE(&stage, "$set", group)
            && bson_append_document_end(&array, &stage) : true)
     && (group ?
            BSON_APPEND_DOCUMENT_BEGIN(&array, UINT8_TO_STR[i], &stage) && ++i
            && BSON_APPEND_AGGREGATE_GROUP_STAGE(&stage, "$group", group)
            && bson_append_document_end(&array, &stage) : true)
     && (options->sort.count == 0 ||
            (BSON_APPEND_DOCUMENT_BEGIN(&array, UINT8_TO_STR[i], &stage) && ++i
            && BSON_APPEND_RBH_FILTER_SORTS(&stage, "$sort",
                                            options->sort.items,
                                            options->sort.count)
            && bson_append_document_end(&array, &stage)))
     && (BSON_APPEND_DOCUMENT_BEGIN(&array, UINT8_TO_STR[i], &stage) && ++i
         && BSON_APPEND_AGGREGATE_PROJECTION_STAGE(&stage, "$project", group,
                                                   output)
         && bson_append_document_end(&array, &stage))
     && (options->skip == 0
      || (BSON_APPEND_DOCUMENT_BEGIN(&array, UINT8_TO_STR[i], &stage) && ++i
       && BSON_APPEND_INT64(&stage, "$skip", options->skip)
       && bson_append_document_end(&array, &stage)))
     && (options->limit == 0
      || (BSON_APPEND_DOCUMENT_BEGIN(&array, UINT8_TO_STR[i], &stage) && ++i
       && BSON_APPEND_INT64(&stage, "$limit", options->limit)
       && bson_append_document_end(&array, &stage)))
     && bson_append_array_end(pipeline, &array))
        return pipeline;

    bson_destroy(pipeline);
    errno = ENOBUFS;
    return NULL;
}

/*----------------------------------------------------------------------------*
 |                               mongo_iterator                               |
 *----------------------------------------------------------------------------*/

struct mongo_iterator {
    struct rbh_mut_iterator iterator;
    mongoc_cursor_t *cursor;
};

enum form_token {
    FT_UNKNOWN,
    FT_FSENTRY,
    FT_MAP,
};

static enum form_token
form_tokenizer(const char *key)
{
    switch (*key++) {
    case 'f': /* fsentry */
        if (strcmp(key, "sentry"))
            break;
        return FT_FSENTRY;
    case 'm': /* map */
        if (strcmp(key, "ap"))
            break;
        return FT_MAP;
    }

    return FT_UNKNOWN;
}

static enum form_token
find_form_token(const bson_t *bson)
{
    static enum form_token token;
    bson_iter_t form_iter;

    if (token != 0)
        return token;

    /* XXX: Mongo's output order is not guaranteed to be the same as specified
     * in the projection stage. Therefore, to know how to convert the output,
     * we must first search the "form" key, which we do in a secondary
     * bson_iter_t to avoid skipping information if the key is the first one.
     */
    if (!bson_iter_init_find(&form_iter, bson, "form"))
        return 0;

    token = form_tokenizer(bson_iter_utf8(&form_iter, NULL));
    return token;
}

static struct rbh_value_map *
init_complete_map(struct rbh_value *id_map,
                  struct rbh_value *content_map)
{
    struct rbh_value_map *complete_map;
    struct rbh_value_pair *pairs;
    /* We currently only need to read the id and content maps */
    int count = (id_map ? 2 : 1);

    pairs = RBH_SSTACK_PUSH(values_sstack, NULL, count * sizeof(*pairs));
    complete_map = RBH_SSTACK_PUSH(values_sstack, NULL, sizeof(*complete_map));

    if (id_map) {
        pairs[0].key = "id";
        pairs[0].value = id_map;
        pairs[1].key = "content";
        pairs[1].value = content_map;
    } else {
        pairs[0].key = "content";
        pairs[0].value = content_map;
    }

    complete_map->pairs = pairs;
    complete_map->count = count;

    return complete_map;
}

static struct rbh_value *
value_from_bson(bson_iter_t *iter, char **buffer, size_t *bufsize)
{
    struct rbh_value *value;

    value = RBH_SSTACK_PUSH(values_sstack, NULL, sizeof(*value));

    if (!bson_iter_rbh_value(iter, value, buffer, bufsize))
        return NULL;

    if (value->type != RBH_VT_MAP)
        return NULL;

    return value;
}

static struct rbh_value_map *
map_from_bson(bson_iter_t *iter)
{
    struct rbh_value *content_map = NULL;
    struct rbh_value_map *complete_map;
    struct rbh_value *id_map = NULL;
    char tmp[8192];
    size_t bufsize;
    char *buffer;

    bufsize = sizeof(tmp);
    buffer = tmp;

    if (values_sstack == NULL) {
        values_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                       sizeof(struct rbh_value *));
        if (values_sstack == NULL)
            goto out;
    }

    while (bson_iter_next(iter)) {
        const char *key = bson_iter_key(iter);

        if (strcmp(key, "_id") == 0) {
            id_map = value_from_bson(iter, &buffer, &bufsize);
            if (id_map == NULL)
                goto out;

        } else if (strcmp(key, "map") == 0) {
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out;

            content_map = value_from_bson(iter, &buffer, &bufsize);
        }
    }

    complete_map = init_complete_map(id_map, content_map);
    if (complete_map == NULL)
        goto out;

    return complete_map;

out:
    errno = EINVAL;
    return NULL;
}

static void *
entry_from_bson(const bson_t *bson)
{
    enum form_token form_token;
    bson_iter_t iter;

    if (!bson_iter_init(&iter, bson)) {
        /* XXX: libbson is not quite clear on why this would happen, the code
         *      makes me think it only happens if `bson' is malformed.
         */
        goto out;
    }

    form_token = find_form_token(bson);
    switch (form_token) {
    case FT_UNKNOWN:
        break;
    case FT_MAP:
        return map_from_bson(&iter);
    case FT_FSENTRY:
        return fsentry_from_bson(&iter);
    }

out:
    errno = EINVAL;
    return NULL;
}

static void *
mongo_iter_next(void *iterator)
{
    struct mongo_iterator *mongo_iter = iterator;
    bson_error_t error;
    const bson_t *doc;

    if (!mongoc_cursor_more(mongo_iter->cursor)) {
        errno = ENODATA;
        return NULL;
    }

    if (mongoc_cursor_next(mongo_iter->cursor, &doc))
        return entry_from_bson(doc);

    if (!mongoc_cursor_error(mongo_iter->cursor, &error)) {
        errno = ENODATA;
        return NULL;
    }

    switch (error.domain) {
    case MONGOC_ERROR_SERVER_SELECTION:
        switch (error.code) {
        case MONGOC_ERROR_SERVER_SELECTION_FAILURE:
            errno = ENOTCONN;
            return NULL;
        }
        break;
    }
    snprintf(rbh_backend_error, sizeof(rbh_backend_error), "%d.%d: %s",
             error.domain, error.code, error.message);
    errno = RBH_BACKEND_ERROR;

    return NULL;
}

static void
mongo_iter_destroy(void *iterator)
{
    struct mongo_iterator *mongo_iter = iterator;

    mongoc_cursor_destroy(mongo_iter->cursor);
    free(mongo_iter);
}

static const struct rbh_mut_iterator_operations MONGO_ITER_OPS = {
    .next = mongo_iter_next,
    .destroy = mongo_iter_destroy,
};

static const struct rbh_mut_iterator MONGO_ITER = {
    .ops = &MONGO_ITER_OPS,
};

static struct mongo_iterator *
mongo_iterator_new(mongoc_cursor_t *cursor)
{
    struct mongo_iterator *mongo_iter;

    mongo_iter = malloc(sizeof(*mongo_iter));
    if (mongo_iter == NULL)
        return NULL;

    mongo_iter->iterator = MONGO_ITER;
    mongo_iter->cursor = cursor;

    return mongo_iter;
}

/*----------------------------------------------------------------------------*
 |                             MONGO_BACKEND_OPS                              |
 *----------------------------------------------------------------------------*/

struct mongo_backend {
    struct rbh_backend backend;
    mongoc_client_t *client;
    mongoc_collection_t *entries;
    mongoc_collection_t *info;
    mongoc_collection_t *log;
};

static int
mongo_get_option(void *backend, unsigned int option, void *data,
                 size_t *data_size);

static int
mongo_set_option(void *backend, unsigned int option, const void *data,
                 size_t data_size);

    /*--------------------------------------------------------------------*
     |                               update                               |
     *--------------------------------------------------------------------*/

static mongoc_bulk_operation_t *
_mongoc_collection_create_bulk_operation(
        mongoc_collection_t *collection, bool ordered,
        mongoc_write_concern_t *write_concern
        )
{
#if MONGOC_CHECK_VERSION(1, 9, 0)
    bson_t opts;

    bson_init(&opts);

    if (!BSON_APPEND_BOOL(&opts, "ordered", ordered)) {
        errno = ENOBUFS;
        return NULL;
    }

    if (write_concern && !mongoc_write_concern_append(write_concern, &opts)) {
        errno = EINVAL;
        return NULL;
    }

    return mongoc_collection_create_bulk_operation_with_opts(collection, &opts);
#else
    return mongoc_collection_create_bulk_operation(collection, ordered,
                                                   write_concern);
#endif
}

static bool
_mongoc_bulk_operation_update_one(mongoc_bulk_operation_t *bulk,
                                  const bson_t *selector, const bson_t *update,
                                  bool upsert)
{
#if MONGOC_CHECK_VERSION(1, 7, 0)
    bson_t opts;

    bson_init(&opts);
    if (!BSON_APPEND_BOOL(&opts, "upsert", upsert)) {
        errno = ENOBUFS;
        return NULL;
    }

    /* TODO: handle errors */
    return mongoc_bulk_operation_update_one_with_opts(bulk, selector, update,
                                                      &opts, NULL);
#else
    mongoc_bulk_operation_update_one(bulk, selector, update, upsert);
    return true;
#endif
}

static bool
_mongoc_bulk_operation_remove_one(mongoc_bulk_operation_t *bulk,
                                  const bson_t *selector)
{
#if MONGOC_CHECK_VERSION(1, 7, 0)
    /* TODO: handle errors */
    return mongoc_bulk_operation_remove_one_with_opts(bulk, selector, NULL,
                                                      NULL);
#else
    mongoc_bulk_operation_remove_one(bulk, selector);
    return true;
#endif
}

static bson_t *
bson_selector_from_fsevent(const struct rbh_fsevent *fsevent)
{
    bson_t *selector = bson_new();
    bson_t namespace;
    bson_t elem_match;

    if (!BSON_APPEND_RBH_ID(selector, MFF_ID, &fsevent->id))
        goto out_bson_destroy;

    if (fsevent->type != RBH_FET_XATTR || fsevent->ns.parent_id == NULL)
        return selector;
    assert(fsevent->ns.name);

    if (BSON_APPEND_DOCUMENT_BEGIN(selector, MFF_NAMESPACE, &namespace)
     && BSON_APPEND_DOCUMENT_BEGIN(&namespace, "$elemMatch", &elem_match)
     && BSON_APPEND_RBH_ID(&elem_match, MFF_PARENT_ID, fsevent->ns.parent_id)
     && BSON_APPEND_UTF8(&elem_match, MFF_NAME, fsevent->ns.name)
     && bson_append_document_end(&namespace, &elem_match)
     && bson_append_document_end(selector, &namespace))
        return selector;

out_bson_destroy:
    bson_destroy(selector);
    errno = ENOBUFS;
    return NULL;
}

static bool
mongo_bulk_append_fsevent(mongoc_bulk_operation_t *bulk,
                          const struct rbh_fsevent *fsevent);

static bool
mongo_bulk_append_unlink_from_link(mongoc_bulk_operation_t *bulk,
                                   const struct rbh_fsevent *link)
{
    const struct rbh_fsevent unlink = {
        .type = RBH_FET_UNLINK,
        .id = {
            .data = link->id.data,
            .size = link->id.size,
        },
        .link = {
            .parent_id = link->link.parent_id,
            .name = link->link.name,
        },
    };

    return mongo_bulk_append_fsevent(bulk, &unlink);
}

static bool
mongo_bulk_append_fsevent(mongoc_bulk_operation_t *bulk,
                          const struct rbh_fsevent *fsevent)
{
    bool upsert = false;
    bson_t *selector;
    bson_t *update;
    bool success;

    selector = bson_selector_from_fsevent(fsevent);
    if (selector == NULL)
        return false;

    switch (fsevent->type) {
    case RBH_FET_DELETE:
        success = _mongoc_bulk_operation_remove_one(bulk, selector);
        break;
    case RBH_FET_LINK:
        success = mongo_bulk_append_unlink_from_link(bulk, fsevent);
        if (!success)
            break;
        __attribute__((fallthrough));
    case RBH_FET_UPSERT:
        upsert = true;
        __attribute__((fallthrough));
    default:
        update = bson_update_from_fsevent(fsevent);
        if (update == NULL) {
            int save_errno = errno;

            bson_destroy(selector);
            errno = save_errno;
            return false;
        }

        success = _mongoc_bulk_operation_update_one(bulk, selector, update,
                                                    upsert);
        bson_destroy(update);
    }
    bson_destroy(selector);

    if (!success)
        /* > returns false if passed invalid arguments */
        errno = EINVAL;
    return success;
}

static ssize_t
mongo_bulk_init_from_fsevents(mongoc_bulk_operation_t *bulk,
                              struct rbh_iterator *fsevents)
{
    int save_errno = errno;
    size_t count = 0;

    do {
        const struct rbh_fsevent *fsevent;

        errno = 0;
        fsevent = rbh_iter_next(fsevents);
        if (fsevent == NULL) {
            if (errno == ENODATA)
                break;

            return -1;
        }

        if (!mongo_bulk_append_fsevent(bulk, fsevent))
            return -1;
        count++;
    } while (true);

    errno = save_errno;
    return count;
}

static ssize_t
mongo_backend_update(void *backend, struct rbh_iterator *fsevents)
{
    struct mongo_backend *mongo = backend;
    mongoc_bulk_operation_t *bulk;
    bson_error_t error;
    ssize_t count;
    bson_t reply;
    uint32_t rc;

    if (fsevents == NULL)
        return 0;

    bulk = _mongoc_collection_create_bulk_operation(mongo->entries, false,
                                                    NULL);
    if (bulk == NULL) {
        /* XXX: from libmongoc's documentation:
         *      > "Errors are propagated when executing the bulk operation"
         *
         * We will just assume any error here is related to memory allocation.
         */
        errno = ENOMEM;
        return -1;
    }

    count = mongo_bulk_init_from_fsevents(bulk, fsevents);
    if (count <= 0) {
        int save_errno = errno;

        /* Executing an empty bulk operation is considered an error by mongoc,
         * which is why we return early in this case too
         */
        mongoc_bulk_operation_destroy(bulk);
        errno = save_errno;
        return count;
    }

    rc = mongoc_bulk_operation_execute(bulk, &reply, &error);
    mongoc_bulk_operation_destroy(bulk);
    if (!rc) {
        int errnum = RBH_BACKEND_ERROR;

        snprintf(rbh_backend_error, sizeof(rbh_backend_error), "mongoc: %s",
                 error.message);
#if MONGOC_CHECK_VERSION(1, 11, 0)
        if (mongoc_error_has_label(&reply, "TransientTransactionError"))
            errnum = EAGAIN;
#endif
        bson_destroy(&reply);
        errno = errnum;
        return -1;
    }
    bson_destroy(&reply);

    return count;
}

    /*--------------------------------------------------------------------*
     |                          insert_metadata                           |
     *--------------------------------------------------------------------*/

static int
mongo_backend_insert_metadata(void *backend, struct rbh_value_map *log_map)
{
    struct mongo_backend *mongo = backend;
    bson_t doc;
    int rc = 0;

    bson_init(&doc);

    if (!bson_append_rbh_value_map(&doc, "sync_metadata", 13, log_map)) {
        fprintf(stderr, "Error while appending rbh_value_map to bson\n");
        rc = 1;
        goto out;
    }

    if (!mongoc_collection_insert_one(mongo->log, &doc, NULL, NULL, NULL)) {
        fprintf(stderr, "Failed to insert sync metadatas inside log\n");
        rc = 1;
    }

out:
    bson_destroy(&doc);

    return rc;
}

    /*--------------------------------------------------------------------*
     |                                root                                |
     *--------------------------------------------------------------------*/

static const struct rbh_filter ROOT_FILTER = {
    .op = RBH_FOP_EQUAL,
    .compare = {
        .field = {
            .fsentry = RBH_FP_PARENT_ID,
        },
        .value = {
            .type = RBH_VT_BINARY,
            .binary = {
                .size = 0,
            },
        },
    },
};

static struct rbh_fsentry *
mongo_root(void *backend, const struct rbh_filter_projection *projection)
{
    return rbh_backend_filter_one(backend, &ROOT_FILTER, projection);
}

    /*--------------------------------------------------------------------*
     |                               filter                               |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
mongo_backend_filter(void *backend, const struct rbh_filter *filter,
                     const struct rbh_filter_options *options,
                     const struct rbh_filter_output *output)
{
    struct mongo_backend *mongo = backend;
    struct mongo_iterator *mongo_iter;
    mongoc_cursor_t *cursor;
    bson_t *pipeline;
    bson_t *opts;

    if (rbh_filter_validate(filter))
        return NULL;

    pipeline = bson_pipeline_creation(filter, NULL, options, output);
    if (pipeline == NULL)
        return NULL;

    opts = options->sort.count > 0 ? BCON_NEW("allowDiskUse", BCON_BOOL(true))
                                   : NULL;
    cursor = mongoc_collection_aggregate(mongo->entries, MONGOC_QUERY_NONE,
                                         pipeline, opts, NULL);
    bson_destroy(opts);
    bson_destroy(pipeline);
    if (cursor == NULL) {
        errno = EINVAL;
        return NULL;
    }

    mongo_iter = mongo_iterator_new(cursor);
    if (mongo_iter == NULL) {
        int save_errno = errno;

        mongoc_cursor_destroy(cursor);
        errno = save_errno;
        return NULL;
    }

    return &mongo_iter->iterator;
}

    /*--------------------------------------------------------------------*
     |                               report                               |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
mongo_backend_report(void *backend, const struct rbh_filter *filter,
                     const struct rbh_group_fields *group,
                     const struct rbh_filter_options *options,
                     const struct rbh_filter_output *output)
{
    struct mongo_backend *mongo = backend;
    struct mongo_iterator *mongo_iter;
    mongoc_cursor_t *cursor;
    bson_t *pipeline;
    bson_t *opts;

    if (rbh_filter_validate(filter))
        return NULL;

    pipeline = bson_pipeline_creation(filter, group, options, output);
    if (pipeline == NULL)
        return NULL;

    opts = options->sort.count > 0 ? BCON_NEW("allowDiskUse", BCON_BOOL(true))
                                   : NULL;
    cursor = mongoc_collection_aggregate(mongo->entries, MONGOC_QUERY_NONE,
                                         pipeline, opts, NULL);
    bson_destroy(opts);
    bson_destroy(pipeline);
    if (cursor == NULL) {
        errno = EINVAL;
        return NULL;
    }

    mongo_iter = mongo_iterator_new(cursor);
    if (mongo_iter == NULL) {
        int save_errno = errno;

        mongoc_cursor_destroy(cursor);
        errno = save_errno;
        return NULL;
    }

    return &mongo_iter->iterator;
}

    /*--------------------------------------------------------------------*
     |                              get_info()                            |
     *--------------------------------------------------------------------*/

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
    int32_t size = 0;
    bson_t *command;
    bson_t reply;

    value = RBH_SSTACK_PUSH(info_sstack, NULL, sizeof(*value));

    command = BCON_NEW("collStats", BCON_UTF8("entries"));

    if (!mongoc_collection_command_simple(mongo->entries, command, NULL,
                                          &reply, &error))
        goto out;

    if (bson_iter_init(&iter, &reply) &&
        bson_iter_find(&iter, stats_to_find)) {
        if (BSON_ITER_HOLDS_INT32(&iter)) {
            size = bson_iter_int32(&iter);
        } else {
            goto out;
        }
    } else {
        goto out;
    }

    value->type = RBH_VT_INT32;
    value->int32 = size;

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
        free(value);
        return 0;
    }

    return 1;
}

static struct rbh_value_map *
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

    if (info_sstack == NULL) {
        info_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                    (sizeof(struct rbh_value_map *)));
        if (!info_sstack)
            goto out;
    }

    pairs = RBH_SSTACK_PUSH(info_sstack, NULL, count * sizeof(*pairs));
    if (!pairs)
        goto out;

    map_value = RBH_SSTACK_PUSH(info_sstack, NULL, sizeof(*map_value));
    if (!map_value)
        goto out;

    if (info_flags & RBH_INFO_AVG_OBJ_SIZE) {
        if (!get_collection_stats(mongo, "avgObjSize", &pairs[idx++]))
            goto out;
    }

    if (info_flags & RBH_INFO_COUNT) {
        if (!get_collection_count(mongo, &pairs[idx++]))
            goto out;
    }

    if (info_flags & RBH_INFO_SIZE) {
        if (!get_collection_stats(mongo, "size", &pairs[idx++]))
            goto out;
    }

    map_value->pairs = pairs;
    map_value->count = idx;

    return map_value;
out:
    errno = EINVAL;
    return NULL;
}

    /*--------------------------------------------------------------------*
     |                              destroy                               |
     *--------------------------------------------------------------------*/

static void
mongo_backend_destroy(void *backend)
{
    struct mongo_backend *mongo = backend;

    mongoc_collection_destroy(mongo->entries);
    mongoc_collection_destroy(mongo->info);
    mongoc_collection_destroy(mongo->log);
    mongoc_client_destroy(mongo->client);
    free(mongo);
}

static struct rbh_backend *
mongo_backend_branch(void *backend, const struct rbh_id *id, const char *path);

static const struct rbh_backend_operations MONGO_BACKEND_OPS = {
    .get_option = mongo_get_option,
    .set_option = mongo_set_option,
    .branch = mongo_backend_branch,
    .root = mongo_root,
    .update = mongo_backend_update,
    .insert_metadata = mongo_backend_insert_metadata,
    .filter = mongo_backend_filter,
    .report = mongo_backend_report,
    .get_info = mongo_backend_get_info,
    .destroy = mongo_backend_destroy,
};

    /*--------------------------------------------------------------------*
     |                             gc_filter                              |
     *--------------------------------------------------------------------*/

static bson_t *
bson_from_options_and_output(const struct rbh_filter_options *options,
                             const struct rbh_filter_output *output)
{
    bson_t *bson;

    if (options->skip > INT64_MAX || options->limit > INT64_MAX) {
        errno = ENOTSUP;
        return NULL;
    }

    bson = bson_new();
    if (BSON_APPEND_AGGREGATE_PROJECTION_STAGE(bson, "projection", NULL, output)
     && (options->skip == 0
      || BSON_APPEND_INT64(bson, "skip", options->skip))
     && (options->limit == 0
      || BSON_APPEND_INT64(bson, "limit", options->limit))
     && (options->sort.count == 0
      || (BSON_APPEND_RBH_FILTER_SORTS(bson, "sort", options->sort.items,
                                       options->sort.count)
       && BSON_APPEND_BOOL(bson, "allowDiskUse", true))))
        return bson;

    bson_destroy(bson);
    errno = ENOBUFS;
    return NULL;
}

static bson_t *
bson_from_gc_filter(const struct rbh_filter *filter_)
{
    bson_t *filter = bson_new();
    bson_t array, subarray;
    bson_t document;
    uint8_t i = 0;

    if (BSON_APPEND_ARRAY_BEGIN(filter, "$and", &array)
     && BSON_APPEND_DOCUMENT_BEGIN(&array, UINT8_TO_STR[i], &document) && ++i
     && BSON_APPEND_ARRAY_BEGIN(&document, MFF_NAMESPACE, &subarray)
     && bson_append_array_end(&document, &subarray)
     && bson_append_document_end(&array, &document)
     && BSON_APPEND_RBH_FILTER(&array, UINT8_TO_STR[i], filter_) && ++i
     && bson_append_array_end(filter, &array))
        return filter;

    bson_destroy(filter);
    errno = ENOBUFS;
    return NULL;
}

static struct rbh_mut_iterator *
mongo_gc_backend_filter(void *backend, const struct rbh_filter *filter_,
                        const struct rbh_filter_options *options,
                        const struct rbh_filter_output *output_)
{
    const unsigned int unavailable_fields =
        RBH_FP_PARENT_ID | RBH_FP_NAME | RBH_FP_NAMESPACE_XATTRS;
    struct rbh_filter_output output = *output_;
    struct mongo_backend *mongo = backend;
    struct mongo_iterator *mongo_iter;
    mongoc_cursor_t *cursor;
    bson_t *filter;
    bson_t *opts;

    if (rbh_filter_validate(filter_))
        return NULL;

    /* Removed unavailable projection fields */
    output.projection.fsentry_mask &= ~unavailable_fields;
    opts = bson_from_options_and_output(options, &output);
    if (opts == NULL)
        return NULL;

    filter = bson_from_gc_filter(filter_);
    if (filter == NULL) {
        int save_errno = errno;

        bson_destroy(opts);
        errno = save_errno;
        return NULL;
    }

    cursor = mongoc_collection_find_with_opts(mongo->entries, filter, opts,
                                              NULL);
    bson_destroy(filter);
    bson_destroy(opts);
    if (cursor == NULL) {
        errno = EINVAL;
        return NULL;
    }

    mongo_iter = mongo_iterator_new(cursor);
    if (mongo_iter == NULL) {
        int save_errno = errno;

        mongoc_cursor_destroy(cursor);
        errno = save_errno;
    }

    return &mongo_iter->iterator;
}

static const struct rbh_backend_operations MONGO_GC_BACKEND_OPS = {
    .get_option = mongo_get_option,
    .set_option = mongo_set_option,
    .root = mongo_root,
    .update = mongo_backend_update,
    .filter = mongo_gc_backend_filter,
    .destroy = mongo_backend_destroy,
};

    /*--------------------------------------------------------------------*
     |                             get_option                             |
     *--------------------------------------------------------------------*/

static int
mongo_get_gc_option(struct mongo_backend *mongo, void *data, size_t *data_size)
{
    bool is_gc = mongo->backend.ops == &MONGO_GC_BACKEND_OPS;

    if (*data_size < sizeof(is_gc)) {
        *data_size = sizeof(is_gc);
        errno = EOVERFLOW;
        return -1;
    }
    memcpy(data, &is_gc, sizeof(is_gc));
    *data_size = sizeof(is_gc);
    return 0;
}

static int
mongo_get_option(void *backend, unsigned int option, void *data,
                 size_t *data_size)
{
    struct mongo_backend *mongo = backend;

    switch (option) {
    case RBH_GBO_GC:
        return mongo_get_gc_option(mongo, data, data_size);
    }

    errno = ENOPROTOOPT;
    return -1;
}

    /*--------------------------------------------------------------------*
     |                             set_option                             |
     *--------------------------------------------------------------------*/

static int
mongo_set_gc_option(struct mongo_backend *mongo, const void *data,
                    size_t data_size)
{
    bool is_gc;

    if (data_size != sizeof(is_gc)) {
        errno = EINVAL;
        return -1;
    }
    memcpy(&is_gc, data, sizeof(is_gc));

    mongo->backend.ops = is_gc ? &MONGO_GC_BACKEND_OPS : &MONGO_BACKEND_OPS;
    return 0;
}

static int
mongo_set_option(void *backend, unsigned int option, const void *data,
                 size_t data_size)
{
    struct mongo_backend *mongo = backend;

    switch (option) {
    case RBH_GBO_GC:
        return mongo_set_gc_option(mongo, data, data_size);
    }

    errno = ENOPROTOOPT;
    return -1;
}

    /*--------------------------------------------------------------------*
     |                               branch                               |
     *--------------------------------------------------------------------*/

struct mongo_branch_backend {
    struct mongo_backend mongo;
    struct rbh_id id;
};

        /*------------------------------------------------------------*
         |                        branch-root                         |
         *------------------------------------------------------------*/

static struct rbh_fsentry *
mongo_branch_root(void *backend, const struct rbh_filter_projection *projection)
{
    struct mongo_branch_backend *branch = backend;
    const struct rbh_filter id_filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID,
            },
            .value = {
                .type = RBH_VT_BINARY,
                .binary = {
                   .data = branch->id.data,
                   .size = branch->id.size,
                },
            },
        },
    };
    const struct rbh_backend_operations *ops = branch->mongo.backend.ops;
    struct rbh_fsentry *root;

    /* To avoid the infinite recursion root -> branch_filter -> root -> ... */
    branch->mongo.backend.ops = &MONGO_BACKEND_OPS;
    root = rbh_backend_filter_one(backend, &id_filter, projection);
    branch->mongo.backend.ops = ops;
    return root;
}

        /*------------------------------------------------------------*
         |                       branch-filter                        |
         *------------------------------------------------------------*/

/* This implementation is almost generic, except for the calls to
 * mongo_backend_filter() (calling rbh_backend_filter() instead is not an option
 * as it would lead to an infinite recursion).
 *
 * If another backend ever needs this code, one should consider putting it in
 * a separate source file, replace calls to mongo_backend_filter() with
 * BACKEND_NAME ## _backend_filter(), and include the code both here and in the
 * other backend's sources, after approprately defining the BACKEND_NAME macro.
 */

enum ringr_reader_type {
    RRT_DIRECTORIES,
    RRT_FSENTRIES,
};

struct branch_iterator {
    struct rbh_mut_iterator iterator;

    struct rbh_backend *backend;
    struct rbh_filter *filter;
    struct rbh_filter_options options;
    struct rbh_filter_output output;

    struct rbh_mut_iterator *directories;
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *directory;

    struct rbh_ringr *ids[2];       /* indexed with enum ringr_reader_type */
    struct rbh_ringr *values[2];    /* indexed with enum ringr_reader_type */
    struct rbh_value value;
};

static enum ringr_reader_type
ringr_largest_reader(struct rbh_ringr *ringr[2])
{
    size_t size[2];

    rbh_ringr_peek(ringr[RRT_DIRECTORIES], &size[0]);
    rbh_ringr_peek(ringr[RRT_FSENTRIES], &size[1]);

    return size[0] > size[1] ? RRT_DIRECTORIES : RRT_FSENTRIES;
}

static struct rbh_mut_iterator *
_filter_child_fsentries(struct rbh_backend *backend, size_t id_count,
                        const struct rbh_value *id_values,
                        const struct rbh_filter *filter,
                        const struct rbh_filter_options *options,
                        const struct rbh_filter_output *output)
{
    const struct rbh_filter parent_id_filter = {
        .op = RBH_FOP_IN,
        .compare = {
            .field = {
                .fsentry = RBH_FP_PARENT_ID,
            },
            .value = {
                .type = RBH_VT_SEQUENCE,
                .sequence = {
                    .count = id_count,
                    .values = id_values,
                }
            },
        },
    };
    const struct rbh_filter *filters[2] = {
        &parent_id_filter,
        filter,
    };
    const struct rbh_filter and_filter = {
        .op = RBH_FOP_AND,
        .logical = {
            .count = 2,
            .filters = filters,
        },
    };

    return mongo_backend_filter(backend, &and_filter, options, output);
}

static struct rbh_mut_iterator *
filter_child_fsentries(struct rbh_backend *backend, struct rbh_ringr *_values,
                       struct rbh_ringr *_ids, const struct rbh_filter *filter,
                       const struct rbh_filter_options *options,
                       const struct rbh_filter_output *output)
{
    struct rbh_mut_iterator *iterator;
    struct rbh_value *values;
    size_t readable;
    size_t count;
    int rc;

    values = rbh_ringr_peek(_values, &readable);
    if (readable == 0) {
        errno = ENODATA;
        return NULL;
    }
    assert(readable % sizeof(*values) == 0);
    count = readable / sizeof(*values);

    iterator = _filter_child_fsentries(backend, count, values, filter, options,
                                       output);
    if (iterator == NULL)
        return NULL;

    /* IDs have variable size, we cannot just ack `count * <something>` bytes */
    readable = 0;
    for (size_t i = 0; i < count; ++i)
        readable += values[i].binary.size;

    /* Acknowledge data in both rings */
    rc = rbh_ringr_ack(_values, count * sizeof(*values));
    assert(rc == 0);
    rbh_ringr_ack(_ids, readable);
    assert(rc == 0);

    return iterator;
}

static const struct rbh_filter ISDIR_FILTER = {
    .op = RBH_FOP_EQUAL,
    .compare = {
        .field = {
            .fsentry = RBH_FP_STATX,
            .statx = RBH_STATX_TYPE,
        },
        .value = {
            .type = RBH_VT_INT32,
            .int32 = S_IFDIR,
        },
    },
};

static int
branch_iter_recurse(struct branch_iterator *iter)
{
    const struct rbh_filter_options OPTIONS = { 0 };
    const struct rbh_filter_output OUTPUT = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = RBH_FP_ID,
        },
    };
    struct rbh_mut_iterator *_directories;
    struct rbh_mut_iterator *directories;

    _directories = filter_child_fsentries(iter->backend,
                                          iter->values[RRT_DIRECTORIES],
                                          iter->ids[RRT_DIRECTORIES],
                                          &ISDIR_FILTER, &OPTIONS, &OUTPUT);
    if (_directories == NULL)
        return -1;

    directories = rbh_mut_iter_chain(iter->directories, _directories);
    if (directories == NULL) {
        int save_errno = errno;

        rbh_mut_iter_destroy(_directories);
        errno = save_errno;
        return -1;
    }
    iter->directories = directories;

    return 0;
}

static struct rbh_mut_iterator *
_branch_next_fsentries(struct branch_iterator *iter)
{
    return filter_child_fsentries(iter->backend, iter->values[RRT_FSENTRIES],
                                  iter->ids[RRT_FSENTRIES], iter->filter,
                                  &iter->options, &iter->output);
}

static struct rbh_mut_iterator *
branch_next_fsentries(struct branch_iterator *iter)
{
    struct rbh_value *value = &iter->value;

    /* A previous call to branch_next_fsentries() may have been interrupted
     * before all the data that needed to be committed actually was.
     *
     * Resume the execution from where it stopped.
     */
    if (iter->directory != NULL) {
        if (iter->value.binary.data != NULL)
            goto record_rbh_value;
        goto record_id;
    }

    /* Build a list of directory ids */
    while (true) {
        const struct rbh_id *id;

        /* Fetch the next directory */
        iter->directory = rbh_mut_iter_next(iter->directories);
        if (iter->directory == NULL) {
            if (errno != ENODATA)
                return NULL;

            /* `iterator->directories' is exhausted, let's hydrate it */
            if (branch_iter_recurse(iter)) {
                if (errno != ENODATA)
                    return NULL;

                /* The traversal is complete */
                return _branch_next_fsentries(iter);
            }
            continue;
        }

record_id:
        id = &iter->directory->id;
        value->binary.data = rbh_ringr_push(*iter->ids, id->data, id->size);
        /* Record this directory for a later traversal */
        if (value->binary.data == NULL) {
            switch (errno) {
            case ENOBUFS: /* the ring is full */
                /* Should we traverse directories or fsentries? */
                switch (ringr_largest_reader(iter->ids)) {
                case RRT_DIRECTORIES:
                    if (branch_iter_recurse(iter)) {
                        /* the ring can't be full and empty at the same time */
                        assert(errno != ENODATA);
                        return NULL;
                    }
                    goto record_id;
                case RRT_FSENTRIES:
                    return _branch_next_fsentries(iter);
                }
                __builtin_unreachable();
            default:
                return NULL;
            }
        }
        value->binary.size = id->size;

        /* Recording the id first helps keep the code's complexity bearable
         *
         * If someone ever wants to do it the other way around, they should
         * keep in mind that:
         *   - there is no guarantee on how long an id is:
         *     1) although it is reasonable to assume an id is only a few bytes
         *        long (at the time of writing, ids are 16 bytes long), there is
         *        no limit to how big "a few bytes" might be
         *     2) different ids may have different sizes
         *   - list_child_fsentries() asssumes all the readable rbh_values
         *     in the value ring contain an id that points inside the id ring
         *   - rbh_ring_push() may fail (that one is obvious)
         *   - no matter the error, branch_next_fsentries() must be retryable
         */

record_rbh_value:
        /* Then, record the associated rbh_value in `iterator->values' */
        if (rbh_ringr_push(*iter->values, value, sizeof(*value)) == NULL) {
            switch (errno) {
            case ENOBUFS: /* the ring is full */
                /* Should we traverse directories or fsentries? */
                switch (ringr_largest_reader(iter->ids)) {
                case RRT_DIRECTORIES:
                    if (branch_iter_recurse(iter)) {
                        /* the ring can't be full and empty at the same time */
                        assert(errno != ENODATA);
                        return NULL;
                    }
                    goto record_rbh_value;
                case RRT_FSENTRIES:
                    return _branch_next_fsentries(iter);
                }
                __builtin_unreachable();
            default:
                return NULL;
            }
        }
        free(iter->directory);
    }
}

static void *
branch_iter_next(void *iterator)
{
    struct branch_iterator *iter = iterator;
    struct rbh_fsentry *fsentry;

    if (iter->fsentries == NULL) {
        iter->fsentries = branch_next_fsentries(iter);
        if (iter->fsentries == NULL)
            return NULL;
    }

    fsentry = rbh_mut_iter_next(iter->fsentries);
    if (fsentry != NULL)
        return fsentry;

    assert(errno);
    if (errno != ENODATA)
        return NULL;

    rbh_mut_iter_destroy(iter->fsentries);
    iter->fsentries = NULL;

    return branch_iter_next(iterator);
}

static void
branch_iter_destroy(void *iterator)
{
    struct branch_iterator *iter = iterator;

    rbh_ringr_destroy(iter->ids[0]);
    rbh_ringr_destroy(iter->ids[1]);
    rbh_ringr_destroy(iter->values[0]);
    rbh_ringr_destroy(iter->values[1]);

    if (iter->fsentries)
        rbh_mut_iter_destroy(iter->fsentries);
    if (iter->directories)
        rbh_mut_iter_destroy(iter->directories);
    free(iter->directory);
    free(iter->filter);
    free(iter);
}

static const struct rbh_mut_iterator_operations BRANCH_ITER_OPS = {
    .next    = branch_iter_next,
    .destroy = branch_iter_destroy,
};

static const struct rbh_mut_iterator BRANCH_ITERATOR = {
    .ops = &BRANCH_ITER_OPS,
};

/* Unlike rbh_backend_filter_one(), this function is about applying a filter to
 * a specific entry (identified by its ID) and see if it matches.
 */
static struct rbh_mut_iterator *
filter_one(void *backend, const struct rbh_id *id,
           const struct rbh_filter *filter,
           const struct rbh_filter_options *options,
           const struct rbh_filter_output *output)
{
    const struct rbh_filter id_filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID,
            },
            .value = {
                .type = RBH_VT_BINARY,
                .binary = {
                    .data = id->data,
                    .size = id->size,
                },
            },
        },
    };
    const struct rbh_filter *filters[2] = {
        &id_filter,
        filter,
    };
    const struct rbh_filter and_filter = {
        .op = RBH_FOP_AND,
        .logical = {
            .count = 2,
            .filters = filters,
        },
    };

    return mongo_backend_filter(backend, &and_filter, options, output);
}

#define VALUE_RING_SIZE (1 << 14) /* 16MB */
#define ID_RING_SIZE (1 << 14) /* 16MB */

struct rbh_mut_iterator *
generic_branch_backend_filter(void *backend, const struct rbh_filter *filter,
                              const struct rbh_filter_options *options,
                              const struct rbh_filter_output *output)
{
    const struct rbh_filter_projection ID_ONLY = {
        .fsentry_mask = RBH_FP_ID,
    };
    struct branch_iterator *iter;
    int save_errno = errno;

    /* The recursive traversal of the branch prevents a few features from
     * working out of the box.
     */
    if (options->skip || options->limit || options->sort.count) {
        errno = ENOTSUP;
        return NULL;
    }

    iter = malloc(sizeof(*iter));
    if (iter == NULL)
        return NULL;

    iter->directory = rbh_backend_root(backend, &ID_ONLY);
    if (iter->directory == NULL) {
        save_errno = errno;
        goto out_free_iter;
    }

    assert(iter->directory->mask & RBH_FP_ID);
    iter->fsentries = filter_one(backend, &iter->directory->id,
                                 filter, options, output);
    if (iter->fsentries == NULL) {
        save_errno = errno;
        goto out_free_directory;
    }

    errno = 0;
    iter->filter = filter ? rbh_filter_clone(filter) : NULL;
    if (iter->filter == NULL && errno != 0) {
        save_errno = errno;
        goto out_destroy_fsentries;
    }
    errno = save_errno;

    iter->values[0] = rbh_ringr_new(VALUE_RING_SIZE);
    if (iter->values[0] == NULL) {
        save_errno = errno;
        goto out_free_filter;
    }

    iter->values[1] = rbh_ringr_dup(iter->values[0]);
    if (iter->values[1] == NULL) {
        save_errno = errno;
        goto out_free_first_values_ringr;
    }

    iter->ids[0] = rbh_ringr_new(ID_RING_SIZE);
    if (iter->ids[0] == NULL) {
        save_errno = errno;
        goto out_free_second_values_ringr;
    }

    iter->ids[1] = rbh_ringr_dup(iter->ids[0]);
    if (iter->ids[1] == NULL) {
        save_errno = errno;
        goto out_free_first_ids_ringr;
    }

    iter->options = *options;
    iter->output = *output;
    iter->backend = backend;
    iter->iterator = BRANCH_ITERATOR;

    /* Setup `iter->value' for the first run of branch_next_fsentries() */
    iter->directories = rbh_mut_iter_array(NULL, 0, 0); /* Empty iterator */
    iter->value.type = RBH_VT_BINARY;
    iter->value.binary.data = NULL;

    return &iter->iterator;

out_free_first_ids_ringr:
    rbh_ringr_destroy(iter->ids[0]);
out_free_second_values_ringr:
    rbh_ringr_destroy(iter->values[1]);
out_free_first_values_ringr:
    rbh_ringr_destroy(iter->values[0]);
out_free_filter:
    free(iter->filter);
out_destroy_fsentries:
    rbh_mut_iter_destroy(iter->fsentries);
out_free_directory:
    free(iter->directory);
out_free_iter:
    free(iter);
    errno = save_errno;
    return NULL;
}

static const struct rbh_backend_operations MONGO_BRANCH_BACKEND_OPS = {
    .branch = mongo_backend_branch,
    .root = mongo_branch_root,
    .update = mongo_backend_update,
    .filter = generic_branch_backend_filter,
    .destroy = mongo_backend_destroy,
};

static const struct rbh_backend MONGO_BRANCH_BACKEND = {
    .id = RBH_BI_MONGO,
    .name = RBH_MONGO_BACKEND_NAME,
    .ops = &MONGO_BRANCH_BACKEND_OPS,
};

static int
mongo_backend_init_from_uri(struct mongo_backend *mongo,
                            const mongoc_uri_t *uri)
{
    const char *db;

    mongo->client = mongoc_client_new_from_uri(uri);
    if (mongo->client == NULL) {
        errno = ENOMEM;
        return -1;
    }

#if MONGOC_CHECK_VERSION(1, 4, 0)
    if (!mongoc_client_set_error_api(mongo->client,
                                     MONGOC_ERROR_API_VERSION_2)) {
        /* Should never happen */
        mongoc_client_destroy(mongo->client);
        errno = EINVAL;
        return -1;
    }
#endif

    db = mongoc_uri_get_database(uri);
    if (db == NULL) {
        mongoc_client_destroy(mongo->client);
        errno = EINVAL;
        return -1;
    }

    mongo->entries = mongoc_client_get_collection(mongo->client, db, "entries");
    if (mongo->entries == NULL) {
        mongoc_client_destroy(mongo->client);
        errno = ENOMEM;
        return -1;
    }

    mongo->info = mongoc_client_get_collection(mongo->client, db, "info");
    if (mongo->info == NULL) {
        mongoc_client_destroy(mongo->client);
        errno = ENOMEM;
        return -1;
    }

    mongo->log = mongoc_client_get_collection(mongo->client, db, "log");
    if (mongo->log == NULL) {
        mongoc_client_destroy(mongo->client);
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

static struct rbh_backend *
mongo_backend_branch(void *backend, const struct rbh_id *id, const char *path)
{
    struct mongo_backend *mongo = backend;
    struct mongo_branch_backend *branch;
    size_t data_size;
    char *data;

    (void) path;

    data_size = id->size;
    branch = malloc(sizeof(*branch) + data_size);
    if (branch == NULL)
        return NULL;
    data = (char *)branch + sizeof(*branch);

    if (mongo_backend_init_from_uri(&branch->mongo,
                                    mongoc_client_get_uri(mongo->client))) {
        int save_errno = errno;

        free(branch);
        errno = save_errno;
        return NULL;
    }

    rbh_id_copy(&branch->id, id, &data, &data_size);
    branch->mongo.backend = MONGO_BRANCH_BACKEND;

    return &branch->mongo.backend;
}

/*----------------------------------------------------------------------------*
 |                               MONGO_BACKEND                                |
 *----------------------------------------------------------------------------*/

static const struct rbh_backend MONGO_BACKEND = {
    .id = RBH_BI_MONGO,
    .name = RBH_MONGO_BACKEND_NAME,
    .ops = &MONGO_BACKEND_OPS,
};

/*----------------------------------------------------------------------------*
 |                          rbh_mongo_backend_new()                           |
 *----------------------------------------------------------------------------*/

#define MONGODB_ADDRESS_KEY "RBH_MONGODB_ADDRESS"

static const char *
get_mongo_addr()
{
    struct rbh_value value = { 0 };
    enum key_parse_result rc;

    rc = rbh_config_find(MONGODB_ADDRESS_KEY, &value, RBH_VT_STRING);
    if (rc == KPR_ERROR)
        return NULL;

    if (rc == KPR_NOT_FOUND)
        value.string = "mongodb://localhost:27017";

    return value.string;
}

static int
mongo_backend_init(struct mongo_backend *mongo, const char *fsname)
{
    mongoc_uri_t *uri;
    int save_errno;
    const char *addr;
    int rc;

    addr = get_mongo_addr();
    if (addr == NULL)
        return -1;

    uri = mongoc_uri_new(addr);
    if (uri == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (!mongoc_uri_set_database(uri, fsname)) {
        mongoc_uri_destroy(uri);
        errno = EINVAL;
        return -1;
    }

    rc = mongo_backend_init_from_uri(mongo, uri);
    save_errno = errno;
    mongoc_uri_destroy(uri);
    errno = save_errno;
    return rc;
}

struct rbh_backend *
rbh_mongo_backend_new(const struct rbh_backend_plugin *self,
                      const char *type,
                      const char *fsname,
                      struct rbh_config *config)
{
    struct mongo_backend *mongo;

    mongo = malloc(sizeof(*mongo));
    if (mongo == NULL)
        return NULL;

    load_rbh_config(config);

    if (mongo_backend_init(mongo, fsname)) {
        int save_errno = errno;

        free(mongo);
        errno = save_errno;
        return NULL;
    }

    mongo->backend = MONGO_BACKEND;

    return &mongo->backend;
}
