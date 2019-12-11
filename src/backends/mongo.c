/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
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
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

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

/*----------------------------------------------------------------------------*
 |                                bson helpers                                |
 *----------------------------------------------------------------------------*/

/* Fentries are stored in mongo using the following layout:
 *
 * {
 *     _id: fsentry.id (BINARY, SUBTYPE_BINARY)
 *
 *     ns: [{
 *         parent: fsentry.parent_id (BINARY, SUBTYPE_BINARY)
 *         name: fsentry.name (UTF8)
 *     }, ...]
 *
 *     symlink: fsentry.symlink (UTF8)
 *
 *     statx: {
 *         blksize: fsentry.statx.stx_blksize (INT32)
 *         nlink: fsentry.statx.stx_nlink (INT32)
 *         uid: fsentry.statx.stx_uid (INT32)
 *         gid: fsentry.statx.stx_gid (INT32)
 *         type: fsentry.statx.stx_mode & S_IFMT (INT32)
 *         mode: fsentry.statx.stx_mode & ~S_IFMT (INT32)
 *         ino: fsentry.statx.stx_ino (INT64)
 *         size: fsentry.statx.stx_size (INT64)
 *         blocks: fsentry.statx.stx_blocks (INT64)
 *         attributes: {
 *             compressed: fsentry.statx.stx_attributes (BOOL)
 *             immutable: fsentry.statx.stx_attributes (BOOL)
 *             append: fsentry.statx.stx_attributes (BOOL)
 *             nodump: fsentry.statx.stx_attributes (BOOL)
 *             encrypted: fsentry.statx.stx_attributes (BOOL)
 *         }
 *
 *         atime: {
 *             sec: fsentry.statx.stx_atime.tv_sec (INT64)
 *             nsec: fsentry.statx.stx_atime.tv_nsec (INT32)
 *         }
 *         btime: {
 *             sec: fsentry.statx.stx_btime.tv_sec (INT64)
 *             nsec: fsentry.statx.stx_btime.tv_nsec (INT32)
 *         }
 *         ctime: {
 *             sec: fsentry.statx.stx_ctime.tv_sec (INT64)
 *             nsec: fsentry.statx.stx_ctime.tv_nsec (INT32)
 *         }
 *         mtime: {
 *             sec: fsentry.statx.stx_mtime.tv_sec (INT64)
 *             nsec: fsentry.statx.stx_mtime.tv_nsec (INT32)
 *         }
 *
 *         rdev: {
 *             major: fsentry.statx.stx_rdev_major (INT32)
 *             minor: fsentry.statx.stx_rdev_minor (INT32)
 *         }
 *
 *         dev: {
 *             major: fsentry.statx.stx_dev_major (INT32)
 *             minor: fsentry.statx.stx_dev_minor (INT32)
 *         }
 *     }
 * }
 */

    /*--------------------------------------------------------------------*
     |                      Mongo Fenstry Properties                      |
     *--------------------------------------------------------------------*/

/* ID */
#define MFP_ID                      "_id"

/* Namespace */
#define MFP_NAMESPACE               "ns"
#define MFP_PARENT_ID               "parent"
#define MFP_NAME                    "name"

/* symlink */
#define MFP_SYMLINK                 "symlink"

/* statx */
#define MFP_STATX                   "statx"
#define MFP_STATX_BLKSIZE           "blksize"
#define MFP_STATX_NLINK             "nlink"
#define MFP_STATX_UID               "uid"
#define MFP_STATX_GID               "gid"
#define MFP_STATX_TYPE              "type"
#define MFP_STATX_MODE              "mode"
#define MFP_STATX_INO               "ino"
#define MFP_STATX_SIZE              "size"
#define MFP_STATX_BLOCKS            "blocks"

/* statx->stx_attributes */
#define MFP_STATX_ATTRIBUTES        "attributes"
#define MFP_STATX_COMPRESSED        "compressed"
#define MFP_STATX_IMMUTABLE         "immutable"
#define MFP_STATX_APPEND            "append"
#define MFP_STATX_NODUMP            "nodump"
#define MFP_STATX_ENCRYPTED         "encrypted"

/* statx_timestamp */
#define MFP_STATX_TIMESTAMP_SEC     "sec"
#define MFP_STATX_TIMESTAMP_NSEC    "nsec"

#define MFP_STATX_ATIME             "atime"
#define MFP_STATX_BTIME             "btime"
#define MFP_STATX_CTIME             "ctime"
#define MFP_STATX_MTIME             "mtime"

/* "statx_device" */
#define MFP_STATX_DEVICE_MAJOR      "major"
#define MFP_STATX_DEVICE_MINOR      "minor"

#define MFP_STATX_RDEV              "rdev"
#define MFP_STATX_DEV               "dev"

static bool
bson_append_rbh_id(bson_t *bson, const char *key, size_t key_length,
                   const struct rbh_id *id)
{
    if (id->size == 0)
        return bson_append_null(bson, key, key_length);
    return bson_append_binary(bson, key, key_length, BSON_SUBTYPE_BINARY,
                              (const unsigned char *)id->data, id->size);
}

#define BSON_APPEND_RBH_ID(bson, key, id) \
    bson_append_rbh_id(bson, key, strlen(key), id)

static bool
bson_append_statx_attributes(bson_t *bson, const char *key, size_t key_length,
                             uint64_t mask, uint64_t attributes)
{
    bson_t document;

    return bson_append_document_begin(bson, key, key_length, &document)
        && (mask & STATX_ATTR_COMPRESSED ?
                BSON_APPEND_BOOL(&document, MFP_STATX_COMPRESSED,
                                 attributes & STATX_ATTR_COMPRESSED) : true)
        && (mask & STATX_ATTR_IMMUTABLE ?
                BSON_APPEND_BOOL(&document, MFP_STATX_IMMUTABLE,
                                 attributes & STATX_ATTR_IMMUTABLE) : true)
        && (mask & STATX_ATTR_APPEND ?
                BSON_APPEND_BOOL(&document, MFP_STATX_APPEND,
                                 attributes & STATX_ATTR_APPEND) : true)
        && (mask & STATX_ATTR_NODUMP ?
                BSON_APPEND_BOOL(&document, MFP_STATX_NODUMP,
                                 attributes & STATX_ATTR_NODUMP) : true)
        && (mask & STATX_ATTR_ENCRYPTED ?
                BSON_APPEND_BOOL(&document, MFP_STATX_ENCRYPTED,
                                 attributes & STATX_ATTR_ENCRYPTED) : true)
        && bson_append_document_end(bson, &document);
}

#define BSON_APPEND_STATX_ATTRIBUTES(bson, key, mask, attributes) \
    bson_append_statx_attributes(bson, key, strlen(key), mask, attributes)

static bool
bson_append_statx_timestamp(bson_t *bson, const char *key, size_t key_length,
                            const struct statx_timestamp *timestamp)
{
    bson_t document;

    return bson_append_document_begin(bson, key, key_length, &document)
        && BSON_APPEND_INT64(&document, MFP_STATX_TIMESTAMP_SEC,
                             timestamp->tv_sec)
        && BSON_APPEND_INT32(&document, MFP_STATX_TIMESTAMP_NSEC,
                             timestamp->tv_nsec)
        && bson_append_document_end(bson, &document);
}

#define BSON_APPEND_STATX_TIMESTAMP(bson, key, timestamp) \
    bson_append_statx_timestamp(bson, key, strlen(key), timestamp)

static bool
bson_append_statx_device(bson_t *bson, const char *key, size_t key_length,
                         uint32_t major, uint32_t minor)
{
    bson_t document;

    return bson_append_document_begin(bson, key, key_length, &document)
        && BSON_APPEND_INT32(&document, MFP_STATX_DEVICE_MAJOR, major)
        && BSON_APPEND_INT32(&document, MFP_STATX_DEVICE_MINOR, minor)
        && bson_append_document_end(bson, &document);
}

#define BSON_APPEND_STATX_DEVICE(bson, key, major, minor) \
    bson_append_statx_device(bson, key, strlen(key), major, minor)

static bool
bson_append_statx(bson_t *bson, const char *key, size_t key_length,
                  const struct statx *statxbuf)
{
    bson_t document;

    return bson_append_document_begin(bson, key, key_length, &document)
        && BSON_APPEND_INT32(&document, MFP_STATX_BLKSIZE,
                             statxbuf->stx_blksize)
        && (statxbuf->stx_mask & STATX_NLINK ?
                BSON_APPEND_INT32(&document, MFP_STATX_NLINK,
                                  statxbuf->stx_nlink) : true)
        && (statxbuf->stx_mask & STATX_UID ?
                BSON_APPEND_INT32(&document, MFP_STATX_UID, statxbuf->stx_uid) :
                true)
        && (statxbuf->stx_mask & STATX_GID ?
                BSON_APPEND_INT32(&document, MFP_STATX_GID, statxbuf->stx_gid) :
                true)
        && (statxbuf->stx_mask & STATX_TYPE ?
                BSON_APPEND_INT32(&document, MFP_STATX_TYPE,
                                  statxbuf->stx_mode & S_IFMT) : true)
        && (statxbuf->stx_mask & STATX_MODE ?
                BSON_APPEND_INT32(&document, MFP_STATX_MODE,
                                  statxbuf->stx_mode & ~S_IFMT) : true)
        && (statxbuf->stx_mask & STATX_INO ?
                BSON_APPEND_INT64(&document, MFP_STATX_INO, statxbuf->stx_ino) :
                true)
        && (statxbuf->stx_mask & STATX_SIZE ?
                BSON_APPEND_INT64(&document, MFP_STATX_SIZE,
                                  statxbuf->stx_size) : true)
        && (statxbuf->stx_mask & STATX_BLOCKS ?
                BSON_APPEND_INT64(&document, MFP_STATX_BLOCKS,
                                  statxbuf->stx_blocks) : true)
        && BSON_APPEND_STATX_ATTRIBUTES(&document,
                                        MFP_STATX_ATTRIBUTES,
                                        statxbuf->stx_attributes_mask,
                                        statxbuf->stx_attributes)
        && (statxbuf->stx_mask & STATX_ATIME ?
                BSON_APPEND_STATX_TIMESTAMP(&document, MFP_STATX_ATIME,
                                            &statxbuf->stx_atime) : true)
        && (statxbuf->stx_mask & STATX_BTIME ?
                BSON_APPEND_STATX_TIMESTAMP(&document, MFP_STATX_BTIME,
                                            &statxbuf->stx_btime) : true)
        && (statxbuf->stx_mask & STATX_CTIME ?
                BSON_APPEND_STATX_TIMESTAMP(&document, MFP_STATX_CTIME,
                                            &statxbuf->stx_ctime) : true)
        && (statxbuf->stx_mask & STATX_MTIME ?
                BSON_APPEND_STATX_TIMESTAMP(&document, MFP_STATX_MTIME,
                                            &statxbuf->stx_mtime) : true)
        && BSON_APPEND_STATX_DEVICE(&document, MFP_STATX_RDEV,
                                    statxbuf->stx_rdev_major,
                                    statxbuf->stx_rdev_minor)
        && BSON_APPEND_STATX_DEVICE(&document, MFP_STATX_DEV,
                                    statxbuf->stx_dev_major,
                                    statxbuf->stx_dev_minor)
        && bson_append_document_end(bson, &document);
}

#define BSON_APPEND_STATX(bson, key, statxbuf) \
    bson_append_statx(bson, key, strlen(key), statxbuf)

    /*--------------------------------------------------------------------*
     |                     bson_selector_from_fsevent                     |
     *--------------------------------------------------------------------*/

bson_t *
bson_selector_from_fsevent(const struct rbh_fsevent *fsevent)
{
    bson_t *selector = bson_new();

    if (BSON_APPEND_RBH_ID(selector, MFP_ID, &fsevent->id))
        return selector;

    bson_destroy(selector);
    errno = ENOBUFS;
    return NULL;
}

    /*--------------------------------------------------------------------*
     |                      bson_update_from_fsevent                      |
     *--------------------------------------------------------------------*/

bson_t *
bson_from_upsert(const struct statx *statxbuf, const char *symlink)
{
    bson_t *bson = bson_new();
    bson_t document;

    if (BSON_APPEND_DOCUMENT_BEGIN(bson, "$set", &document)
     && (statxbuf ? BSON_APPEND_STATX(&document, MFP_STATX, statxbuf) : true)
     && (symlink ? BSON_APPEND_UTF8(&document, MFP_SYMLINK, symlink) : true)
     && bson_append_document_end(bson, &document))
        return bson;

    bson_destroy(bson);
    errno = ENOBUFS;
    return NULL;
}

bson_t *
bson_from_link(const struct rbh_id *parent_id, const char *name)
{
    bson_t *bson = bson_new();
    bson_t document;
    bson_t subdoc;

    if (BSON_APPEND_DOCUMENT_BEGIN(bson, "$addToSet", &document)
     && BSON_APPEND_DOCUMENT_BEGIN(&document, MFP_NAMESPACE, &subdoc)
     && BSON_APPEND_RBH_ID(&subdoc, MFP_PARENT_ID, parent_id)
     && BSON_APPEND_UTF8(&subdoc, MFP_NAME, name)
     && bson_append_document_end(&document, &subdoc)
     && bson_append_document_end(bson, &document))
        return bson;

    bson_destroy(bson);
    errno = ENOBUFS;
    return NULL;
}

bson_t *
bson_from_unlink(const struct rbh_id *parent_id, const char *name)
{
    bson_t *bson = bson_new();
    bson_t document;
    bson_t subdoc;

    if (BSON_APPEND_DOCUMENT_BEGIN(bson, "$pull", &document)
     && BSON_APPEND_DOCUMENT_BEGIN(&document, MFP_NAMESPACE, &subdoc)
     && BSON_APPEND_RBH_ID(&subdoc, MFP_PARENT_ID, parent_id)
     && BSON_APPEND_UTF8(&subdoc, MFP_NAME, name)
     && bson_append_document_end(&document, &subdoc)
     && bson_append_document_end(bson, &document))
        return bson;

    bson_destroy(bson);
    errno = ENOBUFS;
    return NULL;
}

bson_t *
bson_update_from_fsevent(const struct rbh_fsevent *fsevent)
{
    switch (fsevent->type) {
    case RBH_FET_UPSERT:
        return bson_from_upsert(fsevent->upsert.statx, fsevent->upsert.symlink);
    case RBH_FET_LINK:
        return bson_from_link(fsevent->link.parent_id, fsevent->link.name);
    case RBH_FET_UNLINK:
        return bson_from_unlink(fsevent->link.parent_id, fsevent->link.name);
    default:
        errno = EINVAL;
        return NULL;
    }
}

/*----------------------------------------------------------------------------*
 |                             MONGO_BACKEND_OPS                              |
 *----------------------------------------------------------------------------*/

struct mongo_backend {
    struct rbh_backend backend;
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *db;
    mongoc_collection_t *entries;
};

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

static const struct rbh_fsevent *
fsevent_iter_next(struct rbh_iterator *fsevents)
{
    const struct rbh_fsevent *fsevent;
    int save_errno = errno;

    do {
        errno = 0;
        fsevent = rbh_iter_next(fsevents);
    } while (fsevent == NULL && errno == EAGAIN);

    if (errno == 0 || errno == ENODATA)
        errno = save_errno;
    return fsevent;
}

#define fsevent_for_each(fsevent, fsevents) \
    for (fsevent = fsevent_iter_next(fsevents); fsevent != NULL; \
         fsevent = fsevent_iter_next(fsevents))

static ssize_t
mongo_bulk_operation_update(mongoc_bulk_operation_t *bulk,
                            struct rbh_iterator *fsevents)
{
    const struct rbh_fsevent *fsevent;
    int save_errno = errno;
    size_t count = 0;

    errno = 0;
    fsevent_for_each(fsevent, fsevents) {
        bson_t *selector;
        bson_t *update;
        bool success;

        selector = bson_selector_from_fsevent(fsevent);
        if (selector == NULL)
            return -1;

        if (fsevent->type == RBH_FET_DELETE) {
            success = _mongoc_bulk_operation_remove_one(bulk, selector);
        } else {
            bool upsert = fsevent->type != RBH_FET_UNLINK;

            update = bson_update_from_fsevent(fsevent);
            if (update == NULL) {
                save_errno = errno;
                bson_destroy(selector);
                errno = save_errno;
                return -1;
            }

            success = _mongoc_bulk_operation_update_one(bulk, selector, update,
                                                        upsert);
            bson_destroy(update);
        }

        bson_destroy(selector);
        if (!success) {
            /* > returns false if passed invalid arguments */
            errno = EINVAL;
            return -1;
        }
        count++;
    }

    if (errno)
        return -1;
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

    count = mongo_bulk_operation_update(bulk, fsevents);
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
     |                              destroy                               |
     *--------------------------------------------------------------------*/

static void
mongo_backend_destroy(void *backend)
{
    struct mongo_backend *mongo = backend;

    mongoc_collection_destroy(mongo->entries);
    mongoc_database_destroy(mongo->db);
    mongoc_client_destroy(mongo->client);
    mongoc_uri_destroy(mongo->uri);
    free(mongo);
}

static const struct rbh_backend_operations MONGO_BACKEND_OPS = {
    .update = mongo_backend_update,
    .destroy = mongo_backend_destroy,
};

/*----------------------------------------------------------------------------*
 |                               MONGO_BACKEND                                |
 *----------------------------------------------------------------------------*/

static const struct rbh_backend MONGO_BACKEND = {
    .name = RBH_MONGO_BACKEND_NAME,
    .ops = &MONGO_BACKEND_OPS,
};

/*----------------------------------------------------------------------------*
 |                         rbh_mongo_backend_create()                         |
 *----------------------------------------------------------------------------*/

struct rbh_backend *
rbh_mongo_backend_new(const char *fsname)
{
    struct mongo_backend *mongo;
    int save_errno;

    mongo = malloc(sizeof(*mongo));
    if (mongo == NULL) {
        save_errno = errno;
        goto out;
    }
    mongo->backend = MONGO_BACKEND;

    mongo->uri = mongoc_uri_new("mongodb://localhost:27017");
    if (mongo->uri == NULL) {
        save_errno = EINVAL;
        goto out_free_mongo;
    }

    mongo->client = mongoc_client_new_from_uri(mongo->uri);
    if (mongo->client == NULL) {
        save_errno = ENOMEM;
        goto out_mongoc_uri_destroy;
    }

#if MONGOC_CHECK_VERSION(1, 4, 0)
    if (!mongoc_client_set_error_api(mongo->client,
                                     MONGOC_ERROR_API_VERSION_2)) {
        /* Should never happen */
        save_errno = EINVAL;
        goto out_mongoc_client_destroy;
    }
#endif

    mongo->db = mongoc_client_get_database(mongo->client, fsname);
    if (mongo->db == NULL) {
        save_errno = ENOMEM;
        goto out_mongoc_client_destroy;
    }

    mongo->entries = mongoc_database_get_collection(mongo->db, "entries");
    if (mongo->entries == NULL) {
        save_errno = ENOMEM;
        goto out_mongoc_database_destroy;
    }

    return &mongo->backend;

out_mongoc_database_destroy:
    mongoc_database_destroy(mongo->db);
out_mongoc_client_destroy:
    mongoc_client_destroy(mongo->client);
out_mongoc_uri_destroy:
    mongoc_uri_destroy(mongo->uri);
out_free_mongo:
    free(mongo);
out:
    errno = save_errno;
    return NULL;
}
