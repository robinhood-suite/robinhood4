/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
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
#include "robinhood/sstack.h"
#include "robinhood/uri.h"
#include "value.h"
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
    enum form_token token;
    bson_iter_t form_iter;

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

    /* cursor should only be NULL in dry-run mode */
    if (mongo_iter->cursor == NULL || !mongoc_cursor_more(mongo_iter->cursor)) {
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

static void
print_pipeline_and_opts(bson_t *pipeline, bson_t *opts)
{
    char *pipeline_str = NULL;
    char *opts_str = NULL;

    pipeline_str = bson_as_canonical_extended_json(pipeline, NULL);
    if (opts)
        opts_str = bson_as_canonical_extended_json(opts, NULL);

    printf("Pipeline filter = '%s'%s", pipeline_str, opts ? " " : "\n");
    if (opts)
        printf("with options '%s'\n", opts_str);

    free(pipeline_str);
    free(opts_str);
}

static struct rbh_mut_iterator *
_mongo_backend_filter(void *backend, const struct rbh_filter *filter,
                      const struct rbh_group_fields *group,
                      const struct rbh_filter_options *options,
                      const struct rbh_filter_output *output)
{
    struct mongo_backend *mongo = backend;
    struct mongo_iterator *mongo_iter;
    mongoc_cursor_t *cursor = NULL;
    bson_t *pipeline;
    bson_t *opts;

    if (rbh_filter_validate(filter))
        return NULL;

    pipeline = bson_pipeline_creation(filter, group, options, output);
    if (pipeline == NULL)
        return NULL;

    opts = options->sort.count > 0 ? BCON_NEW("allowDiskUse", BCON_BOOL(true))
                                   : NULL;

    if (options->verbose)
        print_pipeline_and_opts(pipeline, opts);

    if (options->dry_run) {
        bson_destroy(opts);
        bson_destroy(pipeline);
        goto skip_aggregate;
    }

    cursor = mongoc_collection_aggregate(mongo->entries, MONGOC_QUERY_NONE,
                                         pipeline, opts, NULL);

    bson_destroy(opts);
    bson_destroy(pipeline);
    if (cursor == NULL) {
        errno = EINVAL;
        return NULL;
    }

skip_aggregate:
    mongo_iter = mongo_iterator_new(cursor);
    if (mongo_iter == NULL) {
        int save_errno = errno;

        mongoc_cursor_destroy(cursor);
        errno = save_errno;
        return NULL;
    }

    return &mongo_iter->iterator;
}

static struct rbh_mut_iterator *
mongo_backend_filter(void *backend, const struct rbh_filter *filter,
                     const struct rbh_filter_options *options,
                     const struct rbh_filter_output *output)
{
    return _mongo_backend_filter(backend, filter, NULL, options, output);
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
    return _mongo_backend_filter(backend, filter, group, options, output);
}

    /*--------------------------------------------------------------------*
     |                              get_info()                            |
     *--------------------------------------------------------------------*/

static int
get_collection_info(const struct mongo_backend *mongo, char *field_to_find,
                    struct rbh_value_pair *pair)
{
    struct rbh_value *value;
    mongoc_cursor_t *cursor;
    char _buffer[4096];
    const bson_t *doc;
    bson_iter_t iter;
    bson_t *filter;
    size_t bufsize;
    char *buffer;
    int rc = 0;

    buffer = _buffer;
    bufsize = sizeof(_buffer);
    value = RBH_SSTACK_PUSH(info_sstack, NULL, sizeof(*value));

    filter = BCON_NEW("_id", "backend_info");
    cursor = mongoc_collection_find_with_opts(mongo->info, filter, NULL, NULL);
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

        if (strcmp(key, field_to_find) == 0) {
            if (!bson_iter_rbh_value(&iter, value, &buffer, &bufsize)) {
                rc = 1;
                goto out;
            }

            pair->key = field_to_find;
            pair->value = value_clone(value);
            break;
        }
    }

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

    if (info_flags & RBH_INFO_BACKEND_SOURCE) {
        if (get_collection_info(mongo, "backend_source", &pairs[idx++]))
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
     |                       insert_backend_source                        |
     *--------------------------------------------------------------------*/

static int
_mongo_insert_source(const struct mongo_backend *mongo,
                     const struct rbh_value *backend_sequence)
{
    mongoc_collection_t *collection;
    bson_t *filter;
    bson_t *opts;
    int rc = 0;

    assert(backend_sequence->type == RBH_VT_SEQUENCE);

    collection = mongo->info;
    filter = BCON_NEW("_id", "backend_info");
    opts = BCON_NEW("upsert", BCON_BOOL(true));

    for (uint8_t i = 0 ; i < backend_sequence->sequence.count ; i++) {
        bson_t backend_source;
        bson_error_t error;
        bson_t *update;
        bool result;

        update = bson_new();

        if (!(BSON_APPEND_DOCUMENT_BEGIN(update, "$addToSet", &backend_source)
              && BSON_APPEND_RBH_VALUE_MAP(
                     &backend_source,
                     "backend_source",
                     &backend_sequence->sequence.values[i].map
                 )
              && bson_append_document_end(update, &backend_source))) {
            bson_destroy(update);
            rc = 1;
            break;
        }

        result = mongoc_collection_update_one(collection, filter, update, opts,
                                              NULL, &error);
        bson_destroy(update);
        if (!result) {
            fprintf(stderr, "Upsert failed: %s\n", error.message);
            rc = 1;
            break;
        }
    }

    bson_destroy(filter);
    bson_destroy(opts);

    return rc;
}

static int
mongo_set_info(void *backend, const struct rbh_value *infos, int flags)
{
    struct mongo_backend *mongo = backend;
    int rc = 0;

    if (flags & RBH_INFO_BACKEND_SOURCE)
        rc = _mongo_insert_source(mongo, infos);

    return rc;
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
    .filter = mongo_backend_filter,
    .report = mongo_backend_report,
    .get_info = mongo_backend_get_info,
    .set_info = mongo_set_info,
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

#define backend_filter mongo_backend_filter
#include "../../generic_branch.c"

static struct rbh_value_map *
mongo_backend_get_info(void *backend, int info_flags);

static struct rbh_value_map *
mongo_branch_get_info(void *backend, int info_flags)
{
    struct mongo_branch_backend *branch = backend;

    return mongo_backend_get_info(&branch->mongo, info_flags);
}

static const struct rbh_backend_operations MONGO_BRANCH_BACKEND_OPS = {
    .branch = mongo_backend_branch,
    .root = mongo_branch_root,
    .update = mongo_backend_update,
    .filter = generic_branch_backend_filter,
    .get_info = mongo_branch_get_info,
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

#define MONGODB_ADDRESS_KEY "address"
#define MONGODB_CURSOR_TIMEOUT "cursor_timeout"

static const char *
get_mongo_addr()
{
    struct rbh_value value = { 0 };
    enum key_parse_result rc;

    rc = rbh_config_find("mongo/"MONGODB_ADDRESS_KEY, &value, RBH_VT_STRING);
    if (rc == KPR_ERROR)
        return NULL;

    if (rc == KPR_NOT_FOUND)
        value.string = "mongodb://localhost:27017";

    return value.string;
}

static int32_t
get_cursor_timeout()
{
    struct rbh_value value = { 0 };
    enum key_parse_result rc;

    rc = rbh_config_find("mongo/"MONGODB_CURSOR_TIMEOUT, &value, RBH_VT_INT32);
    if (rc == KPR_ERROR)
        return -1;

    if ((rc == KPR_FOUND && value.int32 == 0) || rc == KPR_NOT_FOUND)
        value.int32 = INT32_MAX;

    return value.int32;
}

static int
mongo_backend_init(struct mongo_backend *mongo, const struct rbh_uri *uri)
{
    mongoc_uri_t *mongo_uri;
    const char *addr;
    int32_t timeout;
    int save_errno;
    int rc;

    if (uri->authority) {
        /* Per RFC 3986, if authority is given, the host MUST be specified, but
         * the port is optionnal. So no need to revert to a default hostname.
         */
        uint16_t port = (uri->authority->port != 0 ? uri->authority->port :
                                                     27017);

        mongo_uri = mongoc_uri_new_for_host_port(uri->authority->host, port);

        if (strcmp(uri->authority->username, "") &&
            !mongoc_uri_set_username(mongo_uri, uri->authority->username)) {
            mongoc_uri_destroy(mongo_uri);
            errno = EINVAL;
            return -1;
        }

        if (strcmp(uri->authority->password, "") &&
            !mongoc_uri_set_password(mongo_uri, uri->authority->password)) {
            mongoc_uri_destroy(mongo_uri);
            errno = EINVAL;
            return -1;
        }
    } else {
        addr = get_mongo_addr();
        if (addr == NULL)
            return -1;

        mongo_uri = mongoc_uri_new(addr);
        if (mongo_uri == NULL) {
            errno = EINVAL;
            return -1;
        }
    }

    if (!mongoc_uri_set_database(mongo_uri, uri->fsname)) {
        mongoc_uri_destroy(mongo_uri);
        errno = EINVAL;
        return -1;
    }

    timeout = get_cursor_timeout();
    if (timeout == -1)
        return -1;

    if (!mongoc_uri_set_option_as_int32(mongo_uri, MONGOC_URI_SOCKETTIMEOUTMS,
                                        timeout)) {
        mongoc_uri_destroy(mongo_uri);
        errno = EINVAL;
        return -1;
    }

    rc = mongo_backend_init_from_uri(mongo, mongo_uri);
    save_errno = errno;
    mongoc_uri_destroy(mongo_uri);
    errno = save_errno;
    return rc;
}

struct rbh_backend *
rbh_mongo_backend_new(const struct rbh_backend_plugin *self,
                      const struct rbh_uri *uri,
                      struct rbh_config *config,
                      bool read_only)
{
    struct mongo_backend *mongo;

    mongo = malloc(sizeof(*mongo));
    if (mongo == NULL)
        return NULL;

    rbh_config_load(config);

    if (mongo_backend_init(mongo, uri)) {
        int save_errno = errno;

        free(mongo);
        errno = save_errno;
        return NULL;
    }

    mongo->backend = MONGO_BACKEND;

    return &mongo->backend;
}
