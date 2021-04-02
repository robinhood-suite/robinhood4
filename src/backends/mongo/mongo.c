/* This file is part of the RobinHood Library
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

#include <sys/stat.h>

/* This backend uses libmongoc, from the "mongo-c-driver" project to interact
 * with a MongoDB database.
 *
 * The documentation for the project can be found at: https://mongoc.org
 */

#include <bson.h>
#include <mongoc.h>

#include "robinhood/backends/mongo.h"
#include "robinhood/itertools.h"
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

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

    /*--------------------------------------------------------------------*
     |               bson_pipeline_from_filter_and_options                |
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
bson_pipeline_from_filter_and_options(const struct rbh_filter *filter,
                                      const struct rbh_filter_options *options)
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
     && (options->sort.count == 0
      || (BSON_APPEND_DOCUMENT_BEGIN(&array, UINT8_TO_STR[i], &stage) && ++i
       && BSON_APPEND_RBH_FILTER_SORTS(&stage, "$sort", options->sort.items,
                                       options->sort.count)
       && bson_append_document_end(&array, &stage)))
     && BSON_APPEND_DOCUMENT_BEGIN(&array, UINT8_TO_STR[i], &stage) && ++i
     && BSON_APPEND_RBH_FILTER_PROJECTION(&stage, "$project",
                                          &options->projection)
     && bson_append_document_end(&array, &stage)
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

static void *
mongo_iter_next(void *iterator)
{
    struct mongo_iterator *mongo_iter = iterator;
    bson_error_t error;
    const bson_t *doc;

    if (mongoc_cursor_next(mongo_iter->cursor, &doc))
        return fsentry_from_bson(doc);

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

static struct rbh_mut_iterator *
mongo_backend_filter(void *backend, const struct rbh_filter *filter,
                     const struct rbh_filter_options *options)
{
    struct mongo_backend *mongo = backend;
    struct mongo_iterator *mongo_iter;
    mongoc_cursor_t *cursor;
    bson_t *pipeline;
    bson_t *opts;

    if (rbh_filter_validate(filter))
        return NULL;

    pipeline = bson_pipeline_from_filter_and_options(filter, options);
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
     |                              destroy                               |
     *--------------------------------------------------------------------*/

static void
mongo_backend_destroy(void *backend)
{
    struct mongo_backend *mongo = backend;

    mongoc_collection_destroy(mongo->entries);
    mongoc_client_destroy(mongo->client);
    free(mongo);
}

static struct rbh_backend *
mongo_backend_branch(void *backend, const struct rbh_id *id);

static const struct rbh_backend_operations MONGO_BACKEND_OPS = {
    .get_option = mongo_get_option,
    .set_option = mongo_set_option,
    .branch = mongo_backend_branch,
    .root = mongo_root,
    .update = mongo_backend_update,
    .filter = mongo_backend_filter,
    .destroy = mongo_backend_destroy,
};

    /*--------------------------------------------------------------------*
     |                             gc_filter                              |
     *--------------------------------------------------------------------*/

static bson_t *
bson_from_options(const struct rbh_filter_options *options)
{
    bson_t *bson;

    if (options->skip > INT64_MAX || options->limit > INT64_MAX) {
        errno = ENOTSUP;
        return NULL;
    }

    bson = bson_new();
    if (BSON_APPEND_RBH_FILTER_PROJECTION(bson, "projection",
                                          &options->projection)
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
                        const struct rbh_filter_options *options_)
{
    const unsigned int unavailable_fields =
        RBH_FP_PARENT_ID | RBH_FP_NAME | RBH_FP_NAMESPACE_XATTRS;
    struct rbh_filter_options options = *options_;
    struct mongo_backend *mongo = backend;
    struct mongo_iterator *mongo_iter;
    mongoc_cursor_t *cursor;
    bson_t *filter;
    bson_t *opts;

    if (rbh_filter_validate(filter_))
        return NULL;

    /* Removed unavailable projection fields */
    options.projection.fsentry_mask &= ~unavailable_fields;
    opts = bson_from_options(&options);
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

struct branch_iterator {
    struct rbh_mut_iterator iterator;

    struct rbh_backend *backend;
    struct rbh_filter *filter;
    struct rbh_filter_options options;

    struct rbh_mut_iterator *directories;
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *directory;
};

static struct rbh_mut_iterator *
filter_child_fsentries(struct rbh_backend *backend,
                       const struct rbh_id *id, const struct rbh_filter *filter,
                       const struct rbh_filter_options *options)
{
    const struct rbh_filter parent_id_filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_PARENT_ID,
            },
            .value = {
                .type = RBH_VT_BINARY,
                .binary = {
                    .size = id->size,
                    .data = id->data,
                },
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

    return mongo_backend_filter(backend, &and_filter, options);
}

static const struct rbh_filter ISDIR_FILTER = {
    .op = RBH_FOP_EQUAL,
    .compare = {
        .field = {
            .fsentry = RBH_FP_STATX,
            .statx = STATX_TYPE,
        },
        .value = {
            .type = RBH_VT_INT32,
            .int32 = S_IFDIR,
        },
    },
};

static struct rbh_mut_iterator *
branch_next_directory(struct branch_iterator *iter)
{
    const struct rbh_filter_options ID_ONLY = {
        .projection = {
            .fsentry_mask = RBH_FP_ID,
        },
    };
    struct rbh_mut_iterator *_directories;
    struct rbh_mut_iterator *directories;
    struct rbh_mut_iterator *fsentries;

    if (iter->directory == NULL) {
        iter->directory = rbh_mut_iter_next(iter->directories);
        if (iter->directory == NULL)
            return NULL;
        assert(iter->directory->mask & RBH_FP_ID);
    }

    _directories = filter_child_fsentries(iter->backend, &iter->directory->id,
                                          &ISDIR_FILTER, &ID_ONLY);
    if (_directories == NULL)
        return NULL;

    fsentries = filter_child_fsentries(iter->backend, &iter->directory->id,
                                       iter->filter, &iter->options);
    if (fsentries == NULL) {
        int save_errno = errno;

        rbh_mut_iter_destroy(_directories);
        errno = save_errno;
        return NULL;
    }

    directories = rbh_mut_iter_chain(iter->directories, _directories);
    if (directories == NULL) {
        int save_errno = errno;

        rbh_mut_iter_destroy(fsentries);
        rbh_mut_iter_destroy(_directories);
        errno = save_errno;
        return NULL;
    }
    iter->directories = directories;

    free(iter->directory);
    iter->directory = NULL;

    return fsentries;
}

static void *
branch_iter_next(void *iterator)
{
    struct branch_iterator *iter = iterator;
    struct rbh_fsentry *fsentry;

    if (iter->fsentries == NULL) {
        iter->fsentries = branch_next_directory(iter);
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

    if (iter->fsentries != NULL)
        rbh_mut_iter_destroy(iter->fsentries);

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
           const struct rbh_filter_options *options)
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

    return mongo_backend_filter(backend, &and_filter, options);
}

struct rbh_mut_iterator *
generic_branch_backend_filter(void *backend, const struct rbh_filter *filter,
                              const struct rbh_filter_options *options)
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
        free(iter);
        errno = save_errno;
        return NULL;
    }
    assert(iter->directory->mask & RBH_FP_ID);

    iter->fsentries = filter_one(backend, &iter->directory->id,
                                 filter, options);
    if (iter->fsentries == NULL) {
        save_errno = errno;
        free(iter->directory);
        free(iter);
        errno = save_errno;
        return NULL;
    }

    errno = 0;
    iter->filter = filter ? rbh_filter_clone(filter) : NULL;
    if (iter->filter == NULL && errno != 0) {
        save_errno = errno;
        rbh_mut_iter_destroy(iter->fsentries);
        free(iter->directory);
        free(iter);
        errno = save_errno;
        return NULL;
    }
    errno = save_errno;

    iter->options = *options;
    iter->directories = NULL;
    iter->backend = backend;
    iter->iterator = BRANCH_ITERATOR;

    return &iter->iterator;
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

    return 0;
}

static struct rbh_backend *
mongo_backend_branch(void *backend, const struct rbh_id *id)
{
    struct mongo_backend *mongo = backend;
    struct mongo_branch_backend *branch;
    size_t data_size;
    char *data;

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

static int
mongo_backend_init(struct mongo_backend *mongo, const char *fsname)
{
    mongoc_uri_t *uri;
    int save_errno;
    int rc;

    uri = mongoc_uri_new("mongodb://localhost:27017");
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
rbh_mongo_backend_new(const char *fsname)
{
    struct mongo_backend *mongo;

    mongo = malloc(sizeof(*mongo));
    if (mongo == NULL)
        return NULL;

    if (mongo_backend_init(mongo, fsname)) {
        int save_errno = errno;

        free(mongo);
        errno = save_errno;
        return NULL;
    }

    mongo->backend = MONGO_BACKEND;

    return &mongo->backend;
}
