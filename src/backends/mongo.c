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
 *
 * Note that when they are fetched _from_ the database, the "ns" field is
 * unwinded so that we do not have to unwind it ourselves.
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
_bson_append_binary(bson_t *bson, const char *key, size_t key_length,
                    bson_subtype_t subtype, const char *data, uint32_t length)
{
    if (length == 0)
        return bson_append_null(bson, key, key_length);
    return bson_append_binary(bson, key, key_length, subtype,
                              (const uint8_t *)data, length);
}

static bool
bson_append_rbh_id(bson_t *bson, const char *key, size_t key_length,
                   const struct rbh_id *id)
{
    return _bson_append_binary(bson, key, key_length, BSON_SUBTYPE_BINARY,
                               id->data, id->size);
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

    /*--------------------------------------------------------------------*
     |                         fsentry_from_bson                          |
     *--------------------------------------------------------------------*/

enum statx_attributes_token {
    SAT_UNKNOWN,
    SAT_COMPRESSED,
    SAT_IMMUTABLE,
    SAT_APPEND,
    SAT_NODUMP,
    SAT_ENCRYPTED,
};

static enum statx_attributes_token
statx_attributes_tokenizer(const char *key)
{
    switch (*key++) {
    case 'a': /* append */
        if (strcmp(key, "ppend"))
            break;
        return SAT_APPEND;
    case 'c': /* compressed */
        if (strcmp(key, "ompressed"))
            break;
        return SAT_COMPRESSED;
    case 'e': /* encrypted */
        if (strcmp(key, "ncrypted"))
            break;
        return SAT_ENCRYPTED;
    case 'i': /* immutable */
        if (strcmp(key, "mmutable"))
            break;
        return SAT_IMMUTABLE;
    case 'n': /* nodump */
        if (strcmp(key, "odump"))
            break;
        return SAT_NODUMP;
    }
    return SAT_UNKNOWN;
}

static bool
statx_attributes_from_bson_iter(bson_iter_t *iter, uint64_t *mask,
                                uint64_t *attributes)
{
    while (bson_iter_next(iter)) {
        switch (statx_attributes_tokenizer(bson_iter_key(iter))) {
        case SAT_UNKNOWN:
            break;
        case SAT_COMPRESSED:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= STATX_ATTR_COMPRESSED;
            else
                *attributes &= ~STATX_ATTR_COMPRESSED;
            *mask |= STATX_ATTR_COMPRESSED;
            break;
        case SAT_IMMUTABLE:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= STATX_ATTR_IMMUTABLE;
            else
                *attributes &= ~STATX_ATTR_IMMUTABLE;
            *mask |= STATX_ATTR_IMMUTABLE;
            break;
        case SAT_APPEND:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= STATX_ATTR_APPEND;
            else
                *attributes &= ~STATX_ATTR_APPEND;
            *mask |= STATX_ATTR_APPEND;
            break;
        case SAT_NODUMP:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= STATX_ATTR_NODUMP;
            else
                *attributes &= ~STATX_ATTR_NODUMP;
            *mask |= STATX_ATTR_NODUMP;
            break;
        case SAT_ENCRYPTED:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= STATX_ATTR_ENCRYPTED;
            else
                *attributes &= ~STATX_ATTR_ENCRYPTED;
            *mask |= STATX_ATTR_ENCRYPTED;
            break;
        }
    }

    return true;

out_einval:
    errno = EINVAL;
    return false;
}

enum statx_timestamp_token {
    STT_UNKNOWN,
    STT_SEC,
    STT_NSEC,
};

static enum statx_timestamp_token
statx_timestamp_tokenizer(const char *key)
{
    switch (*key++) {
    case 'n': /* nsec */
        if (strcmp(key, "sec"))
            break;
        return STT_NSEC;
    case 's': /* sec */
        if (strcmp(key, "ec"))
            break;
        return STT_SEC;
    }
    return STT_UNKNOWN;
}

static bool
statx_timestamp_from_bson_iter(bson_iter_t *iter,
                               struct statx_timestamp *timestamp)
{
    struct { /* mandatory fields */
        bool sec:1;
        bool nsec:1;
    } seen;

    memset(&seen, 0, sizeof(seen));

    while (bson_iter_next(iter)) {
        switch (statx_timestamp_tokenizer(bson_iter_key(iter))) {
        case STT_UNKNOWN:
            break;
        case STT_SEC:
            if (!BSON_ITER_HOLDS_INT64(iter))
                goto out_einval;
            timestamp->tv_sec = bson_iter_int64(iter);
            seen.sec = true;
            break;
        case STT_NSEC:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            timestamp->tv_nsec = bson_iter_int32(iter);
            seen.nsec = true;
            break;
        }
    }

    errno = EINVAL;
    return seen.sec && seen.nsec;

out_einval:
    errno = EINVAL;
    return false;
}

enum statx_device_token {
    SDT_UNKNOWN,
    SDT_MAJOR,
    SDT_MINOR,
};

static enum statx_device_token
statx_device_tokenizer(const char *key)
{
    if (*key++ != 'm')
        return SDT_UNKNOWN;

    switch (*key++) {
    case 'a': /* major */
        if (strcmp(key, "jor"))
            break;
        return SDT_MAJOR;
    case 'i': /* minor */
        if (strcmp(key, "nor"))
            break;
        return SDT_MINOR;
    }
    return SDT_UNKNOWN;
}

static bool
statx_device_from_bson_iter(bson_iter_t *iter, uint32_t *major, uint32_t *minor)
{
    struct { /* mandatory fields */
        bool major:1;
        bool minor:1;
    } seen;

    memset(&seen, 0, sizeof(seen));

    while (bson_iter_next(iter)) {
        switch (statx_device_tokenizer(bson_iter_key(iter))) {
        case SDT_UNKNOWN:
            break;
        case SDT_MAJOR:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            *major = bson_iter_int32(iter);
            seen.major = true;
            break;
        case SDT_MINOR:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            *minor = bson_iter_int32(iter);
            seen.minor = true;
            break;
        }
    }

    errno = EINVAL;
    return seen.major && seen.minor;

out_einval:
    errno = EINVAL;
    return false;
}

enum statx_token {
    ST_UNKNOWN,
    ST_BLKSIZE,
    ST_NLINK,
    ST_UID,
    ST_GID,
    ST_TYPE,
    ST_MODE,
    ST_INO,
    ST_SIZE,
    ST_BLOCKS,
    ST_ATTRIBUTES,
    ST_ATIME,
    ST_BTIME,
    ST_CTIME,
    ST_MTIME,
    ST_RDEV,
    ST_DEV,
};

static enum statx_token
statx_tokenizer(const char *key)
{
    switch (*key++) {
    case 'a': /* atime, attributes */
        if (*key++ != 't')
            break;
        switch (*key++) {
        case 'i': /* atime */
            if (strcmp(key, "me"))
                break;
            return ST_ATIME;
        case 't': /* attributes */
            if (strcmp(key, "ributes"))
                break;
            return ST_ATTRIBUTES;
        }
        break;
    case 'b': /* blksize, blocks, btime */
        switch (*key++) {
        case 'l': /* blksize, blocks */
            switch (*key++) {
            case 'k': /* blksize */
                if (strcmp(key, "size"))
                    break;
                return ST_BLKSIZE;
            case 'o': /* blocks */
                if (strcmp(key, "cks"))
                    break;
                return ST_BLOCKS;
            }
            break;
        case 't': /* btime */
            if (strcmp(key, "ime"))
                break;
            return ST_BTIME;
        }
        break;
    case 'c': /* ctime */
        if (strcmp(key, "time"))
            break;
        return ST_CTIME;
    case 'd': /* dev */
        if (strcmp(key, "ev"))
            break;
        return ST_DEV;
    case 'g': /* gid */
        if (strcmp(key, "id"))
            break;
        return ST_GID;
    case 'i': /* ino */
        if (strcmp(key, "no"))
            break;
        return ST_INO;
    case 'm': /* mode, mtime */
        switch (*key++) {
        case 'o': /* mode */
            if (strcmp(key, "de"))
                break;
            return ST_MODE;
        case 't': /* mtime */
            if (strcmp(key, "ime"))
                break;
            return ST_MTIME;
        }
        break;
    case 'n': /* nlink */
        if (strcmp(key, "link"))
            break;
        return ST_NLINK;
    case 'r': /* rdev */
        if (strcmp(key, "dev"))
            break;
        return ST_RDEV;
    case 's': /* size */
        if (strcmp(key, "ize"))
            break;
        return ST_SIZE;
    case 't': /* type */
        if (strcmp(key, "ype"))
            break;
        return ST_TYPE;
    case 'u': /* uid */
        if (strcmp(key, "id"))
            break;
        return ST_UID;
    }
    return ST_UNKNOWN;
}

static bool
statx_from_bson_iter(bson_iter_t *iter, struct statx *statxbuf)
{
    struct { /* mandatory fields */
        bool blksize:1;
        bool rdev:1;
        bool dev:1;
    } seen;

    memset(&seen, 0, sizeof(seen));
    memset(statxbuf, 0, sizeof(*statxbuf));

    while (bson_iter_next(iter)) {
        bson_iter_t subiter;

        switch (statx_tokenizer(bson_iter_key(iter))) {
        case ST_UNKNOWN:
            break;
        case ST_BLKSIZE:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_blksize = bson_iter_int32(iter);
            seen.blksize = true;
            break;
        case ST_NLINK:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_nlink = bson_iter_int32(iter);
            statxbuf->stx_mask |= STATX_NLINK;
            break;
        case ST_UID:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_uid = bson_iter_int32(iter);
            statxbuf->stx_mask |= STATX_UID;
            break;
        case ST_GID:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_gid = bson_iter_int32(iter);
            statxbuf->stx_mask |= STATX_GID;
            break;
        case ST_TYPE:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_mode |= bson_iter_int32(iter) & S_IFMT;
            statxbuf->stx_mask |= STATX_TYPE;
            break;
        case ST_MODE:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_mode |= bson_iter_int32(iter) & ~S_IFMT;
            statxbuf->stx_mask |= STATX_MODE;
            break;
        case ST_INO:
            if (!BSON_ITER_HOLDS_INT64(iter))
                goto out_einval;
            statxbuf->stx_ino = bson_iter_int64(iter);
            statxbuf->stx_mask |= STATX_INO;
            break;
        case ST_SIZE:
            if (!BSON_ITER_HOLDS_INT64(iter))
                goto out_einval;
            statxbuf->stx_size = bson_iter_int64(iter);
            statxbuf->stx_mask |= STATX_SIZE;
            break;
        case ST_BLOCKS:
            if (!BSON_ITER_HOLDS_INT64(iter))
                goto out_einval;
            statxbuf->stx_blocks = bson_iter_int64(iter);
            statxbuf->stx_mask |= STATX_BLOCKS;
            break;
        case ST_ATTRIBUTES:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            static_assert(
                    sizeof(statxbuf->stx_attributes_mask) == sizeof(uint64_t),
                    ""
                    );
            static_assert(
                    sizeof(statxbuf->stx_attributes) == sizeof(uint64_t),
                    ""
                    );
            if (!statx_attributes_from_bson_iter(
                        &subiter, (uint64_t *)&statxbuf->stx_attributes_mask,
                        (uint64_t *)&statxbuf->stx_attributes
                        ))
                return false;
            break;
        case ST_ATIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!statx_timestamp_from_bson_iter(&subiter, &statxbuf->stx_atime))
                return false;
            statxbuf->stx_mask |= STATX_ATIME;
            break;
        case ST_BTIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!statx_timestamp_from_bson_iter(&subiter, &statxbuf->stx_btime))
                return false;
            statxbuf->stx_mask |= STATX_BTIME;
            break;
        case ST_CTIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!statx_timestamp_from_bson_iter(&subiter, &statxbuf->stx_ctime))
                return false;
            statxbuf->stx_mask |= STATX_CTIME;
            break;
        case ST_MTIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!statx_timestamp_from_bson_iter(&subiter, &statxbuf->stx_mtime))
                return false;
            statxbuf->stx_mask |= STATX_MTIME;
            break;
        case ST_RDEV:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!statx_device_from_bson_iter(&subiter,
                                             &statxbuf->stx_rdev_major,
                                             &statxbuf->stx_rdev_minor))
                return false;
            seen.rdev = true;
            break;
        case ST_DEV:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!statx_device_from_bson_iter(&subiter, &statxbuf->stx_dev_major,
                                             &statxbuf->stx_dev_minor))
                return false;
            seen.dev = true;
            break;
        }
    }

    errno = EINVAL;
    return seen.blksize && seen.rdev && seen.dev;

out_einval:
    errno = EINVAL;
    return false;
}

enum namespace_token {
    NT_UNKNOWN,
    NT_PARENT,
    NT_NAME,
};

static enum namespace_token
namespace_tokenizer(const char *key)
{
    switch (*key++) {
    case 'n': /* name */
        if (strcmp(key, "ame"))
            break;
        return NT_NAME;
    case 'p': /* parent */
        if (strcmp(key, "arent"))
            break;
        return NT_PARENT;
    }
    return NT_UNKNOWN;
}

enum fsentry_token {
    FT_UNKNOWN,
    FT_ID,
    FT_NAMESPACE,
    FT_SYMLINK,
    FT_STATX,
};

static enum fsentry_token
fsentry_tokenizer(const char *key)
{
    switch (*key++) {
    case '_': /* _id */
        if (strcmp(key, "id"))
            break;
        return FT_ID;
    case 'n': /* ns */
        if (strcmp(key, "s"))
            break;
        return FT_NAMESPACE;
    case 's': /* statx, symlink */
        switch (*key++) {
        case 't': /* statx */
            if (strcmp(key, "atx"))
                break;
            return FT_STATX;
        case 'y': /* symlink */
            if (strcmp(key, "mlink"))
                break;
            return FT_SYMLINK;
        }
        break;
    }
    return FT_UNKNOWN;
}

static bool
bson_iter_rbh_id(bson_iter_t *iter, struct rbh_id *id)
{
    const bson_value_t *value = bson_iter_value(iter);

    if (value->value.v_binary.subtype != BSON_SUBTYPE_BINARY) {
        errno = EINVAL;
        return false;
    }

    id->data = (char *)value->value.v_binary.data;
    id->size = value->value.v_binary.data_len;
    return true;
}

static const struct rbh_id PARENT_ROOT_ID = {
    .data = NULL,
    .size = 0,
};

static struct rbh_fsentry *
fsentry_from_bson_iter(bson_iter_t *iter)
{
    struct rbh_id id;
    struct rbh_id parent_id;
    const char *name = NULL;
    const char *symlink = NULL;
    struct statx statxbuf;
    struct {
        bool id:1;
        bool parent:1;
        bool statx:1;
    } seen;

    memset(&seen, 0, sizeof(seen));

    while (bson_iter_next(iter)) {
        bson_iter_t subiter;

        switch (fsentry_tokenizer(bson_iter_key(iter))) {
        case FT_UNKNOWN:
            break;
        case FT_ID:
            if (!BSON_ITER_HOLDS_BINARY(iter) || !bson_iter_rbh_id(iter, &id))
                goto out_einval;
            seen.id = true;
            break;
        case FT_NAMESPACE:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);

            /* The parsing of the namespace subdocument is be done here not to
             * allocate memory on the heap and to simplify the handling of a
             * missing "parent" field.
             */
            while (bson_iter_next(&subiter)) {
                switch (namespace_tokenizer(bson_iter_key(&subiter))) {
                case NT_UNKNOWN:
                    break;
                case NT_PARENT:
                    if (BSON_ITER_HOLDS_NULL(&subiter))
                        parent_id = PARENT_ROOT_ID;
                    else if (!BSON_ITER_HOLDS_BINARY(&subiter)
                            || !bson_iter_rbh_id(&subiter, &parent_id))
                        goto out_einval;
                    seen.parent = true;
                    break;
                case NT_NAME:
                    if (!BSON_ITER_HOLDS_UTF8(&subiter))
                        goto out_einval;
                    name = bson_iter_utf8(&subiter, NULL);
                    break;
                }
            }
            break;
        case FT_SYMLINK:
            if (!BSON_ITER_HOLDS_UTF8(iter))
                goto out_einval;
            symlink = bson_iter_utf8(iter, NULL);
            break;
        case FT_STATX:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!statx_from_bson_iter(&subiter, &statxbuf))
                return false;
            seen.statx = true;
            break;
        }
    }

    return rbh_fsentry_new(seen.id ? &id : NULL,
                           seen.parent ? &parent_id : NULL, name,
                           seen.statx ? &statxbuf : NULL, symlink);

out_einval:
    errno = EINVAL;
    return false;
}

static struct rbh_fsentry *
fsentry_from_bson(const bson_t *bson)
{
    bson_iter_t iter;

    if (!bson_iter_init(&iter, bson)) {
        /* XXX: libbson is not quite clear on why this would happen, the code
         *      makes me think it only happens if `bson' is malformed.
         */
        errno = EINVAL;
        return NULL;
    }

    return fsentry_from_bson_iter(&iter);
}

    /*--------------------------------------------------------------------*
     |                         bson_append_filter                         |
     *--------------------------------------------------------------------*/

/* The following helpers should only be used on a valid filter */

static const char * const FOP2STR[] = {
    [RBH_FOP_EQUAL]             = "$eq",
    [RBH_FOP_LOWER_THAN]        = "$lt",
    [RBH_FOP_LOWER_OR_EQUAL]    = "$le",
    [RBH_FOP_GREATER_THAN]      = "$gt",
    [RBH_FOP_GREATER_OR_EQUAL]  = "$ge",
    [RBH_FOP_IN]                = "$in",
    [RBH_FOP_REGEX]             = "$regex",
    [RBH_FOP_BITS_ANY_SET]      = "$bitsAnySet",
    [RBH_FOP_BITS_ALL_SET]      = "$bitsAllSet",
    [RBH_FOP_BITS_ANY_CLEAR]    = "$bitsAnyClear",
    [RBH_FOP_BITS_ALL_CLEAR]    = "$bitsAllClear",
    [RBH_FOP_AND]               = "$and",
    [RBH_FOP_OR]                = "$or",
};

static const char * const NEGATED_FOP2STR[] = {
    [RBH_FOP_EQUAL]             = "$ne",
    [RBH_FOP_LOWER_THAN]        = "$ge",
    [RBH_FOP_LOWER_OR_EQUAL]    = "$gt",
    [RBH_FOP_GREATER_THAN]      = "$le",
    [RBH_FOP_GREATER_OR_EQUAL]  = "$lt",
    [RBH_FOP_IN]                = "$nin",
    [RBH_FOP_REGEX]             = "$not", /* This is not a mistake */
    [RBH_FOP_BITS_ANY_SET]      = "$bitsAllClear",
    [RBH_FOP_BITS_ALL_SET]      = "$bitsAnyClear",
    [RBH_FOP_BITS_ANY_CLEAR]    = "$bitsAllSet",
    [RBH_FOP_BITS_ALL_CLEAR]    = "$bitsAnySet",
    [RBH_FOP_AND]               = "$or",
    [RBH_FOP_OR]                = "$and",
};

static const char *
fop2str(enum rbh_filter_operator op, bool negate)
{
    return negate ? NEGATED_FOP2STR[op] : FOP2STR[op];
}

static const char * const FIELD2STR[] = {
    [RBH_FF_ID]         = MFP_ID,
    [RBH_FF_PARENT_ID]  = MFP_NAMESPACE "." MFP_PARENT_ID,
    [RBH_FF_NAME]       = MFP_NAMESPACE "." MFP_NAME,
    [RBH_FF_TYPE]       = MFP_STATX "." MFP_STATX_TYPE,
    [RBH_FF_ATIME]      = MFP_STATX "." MFP_STATX_ATIME "." MFP_STATX_TIMESTAMP_SEC,
    [RBH_FF_CTIME]      = MFP_STATX "." MFP_STATX_CTIME "." MFP_STATX_TIMESTAMP_SEC,
    [RBH_FF_MTIME]      = MFP_STATX "." MFP_STATX_MTIME "." MFP_STATX_TIMESTAMP_SEC,
};

static bool
bson_append_filter_value(bson_t *bson, const char *key, size_t key_length,
                         const struct rbh_filter_value *value);

static bool
bson_append_list(bson_t *bson, const char *key, size_t key_length,
                 const struct rbh_filter_value *values, size_t count)
{
    bson_t array;

    if (!bson_append_array_begin(bson, key, key_length, &array))
        return false;

    for (uint32_t i = 0; i < count; i++) {
        char str[16];

        key_length = bson_uint32_to_string(i, &key, str, sizeof(str));
        if (!bson_append_filter_value(&array, key, key_length, &values[i]))
            return false;
    }

    return bson_append_array_end(bson, &array);
}

static bool
_bson_append_regex(bson_t *bson, const char *key, size_t key_length,
                    const char *regex, unsigned int options)
{
    char mongo_regex_options[8] = {'s', '\0',};
    uint8_t i = 1;

    if (options & RBH_FRO_CASE_INSENSITIVE)
        mongo_regex_options[i++] = 'i';

    return bson_append_regex(bson, key, key_length, regex, mongo_regex_options);
}

static bool
bson_append_filter_value(bson_t *bson, const char *key, size_t key_length,
                         const struct rbh_filter_value *value)
{
    switch (value->type) {
    case RBH_FVT_BINARY:
        return _bson_append_binary(bson, key, key_length, BSON_SUBTYPE_BINARY,
                                   value->binary.data, value->binary.size);
    case RBH_FVT_INT32:
        return bson_append_int32(bson, key, key_length, value->int32);
    case RBH_FVT_INT64:
        return bson_append_int64(bson, key, key_length, value->int64);
    case RBH_FVT_STRING:
        return bson_append_utf8(bson, key, key_length, value->string,
                                strlen(value->string));
    case RBH_FVT_REGEX:
        return _bson_append_regex(bson, key, key_length, value->regex.string,
                                   value->regex.options);
    case RBH_FVT_TIME:
        /* Since we do not use the native MongoDB date type, we must adapt our
         * comparison operators.
         */
        return bson_append_int64(bson, key, key_length, value->time);
    case RBH_FVT_LIST:
        return bson_append_list(bson, key, key_length, value->list.elements,
                                value->list.count);
    }
    __builtin_unreachable();
}

#define BSON_APPEND_FILTER_VALUE(bson, key, filter_value) \
    bson_append_filter_value(bson, key, strlen(key), filter_value)

static bool
bson_append_comparison_filter(bson_t *bson, const struct rbh_filter *filter,
                              bool negate)
{
    bson_t document;

    if (filter->op == RBH_FOP_REGEX && !negate)
        /* The regex operator is tricky: $not and $regex are not compatible.
         *
         * The workaround is not to use the $regex operator and replace it with
         * the "/pattern/" syntax.
         */
        return BSON_APPEND_FILTER_VALUE(bson, FIELD2STR[filter->compare.field],
                                        &filter->compare.value);

    return BSON_APPEND_DOCUMENT_BEGIN(bson, FIELD2STR[filter->compare.field],
                                      &document)
        && BSON_APPEND_FILTER_VALUE(&document, fop2str(filter->op, negate),
                                    &filter->compare.value)
        && bson_append_document_end(bson, &document);
}

static bool
_bson_append_filter(bson_t *bson, const struct rbh_filter *filter, bool negate);

static bool
bson_append_filter(bson_t *bson, const char *key, size_t key_length,
                    const struct rbh_filter *filter, bool negate)
{
    bson_t document;

    return bson_append_document_begin(bson, key, key_length, &document)
        && _bson_append_filter(&document, filter, negate)
        && bson_append_document_end(bson, &document);
}
#define BSON_APPEND_FILTER(bson, key, filter) \
    bson_append_filter(bson, key, strlen(key), filter, false)

static bool
bson_append_logical_filter(bson_t *bson, const struct rbh_filter *filter,
                           bool negate)
{
    bson_t array;

    if (filter->op == RBH_FOP_NOT)
        return _bson_append_filter(bson, filter->logical.filters[0], !negate);

    if (!BSON_APPEND_ARRAY_BEGIN(bson, fop2str(filter->op, negate), &array))
        return false;

    for (uint32_t i = 0; i < filter->logical.count; i++) {
        const char *key;
        size_t length;
        char str[16];

        length = bson_uint32_to_string(i, &key, str, sizeof(str));
        if (!bson_append_filter(&array, key, length, filter->logical.filters[i],
                                negate))
            return false;
    }

    return bson_append_array_end(bson, &array);
}

static bool
bson_append_null_filter(bson_t *bson, bool negate)
{
    bson_t document;

    /* XXX: I would prefer to use {$expr: !negate}, but it is not
     *      supported on servers until version 3.6.
     */
    return BSON_APPEND_DOCUMENT_BEGIN(bson, "_id", &document)
        && BSON_APPEND_BOOL(&document, "$exists", !negate)
        && bson_append_document_end(bson, &document);
}

static bool
_bson_append_filter(bson_t *bson, const struct rbh_filter *filter, bool negate)
{
    if (filter == NULL)
        return bson_append_null_filter(bson, negate);

    if (rbh_is_comparison_operator(filter->op))
        return bson_append_comparison_filter(bson, filter, negate);
    return bson_append_logical_filter(bson, filter, negate);
}

    /*--------------------------------------------------------------------*
     |                     bson_pipeline_from_filter                      |
     *--------------------------------------------------------------------*/

static bson_t *
bson_pipeline_from_filter(const struct rbh_filter *filter)
{
    bson_t *pipeline = bson_new();
    bson_t array;
    bson_t stage;

    if (BSON_APPEND_ARRAY_BEGIN(pipeline, "pipeline", &array)
     && BSON_APPEND_DOCUMENT_BEGIN(&array, "0", &stage)
     && BSON_APPEND_UTF8(&stage, "$unwind", "$" MFP_NAMESPACE)
     && bson_append_document_end(&array, &stage)
     && BSON_APPEND_DOCUMENT_BEGIN(&array, "1", &stage)
     && BSON_APPEND_FILTER(&stage, "$match", filter)
     && bson_append_document_end(&array, &stage)
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
    int save_errno = errno;
    const bson_t *doc;

    errno = 0;
    if (mongoc_cursor_next(mongo_iter->cursor, &doc)) {
        errno = save_errno;
        return fsentry_from_bson(doc);
    }

    errno = errno ? : ENODATA;
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

    errno = 0;
    fsevent = rbh_iter_next(fsevents);
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
     |                                root                                |
     *--------------------------------------------------------------------*/

static const struct rbh_filter ROOT_FILTER = {
    .op = RBH_FOP_EQUAL,
    .compare = {
        .field = RBH_FF_PARENT_ID,
        .value = {
            .type = RBH_FVT_BINARY,
            .binary = {
                .size = 0,
            },
        },
    },
};

static struct rbh_fsentry *
mongo_root(void *backend, unsigned int fsentry_mask, unsigned int statx_mask)
{
    return rbh_backend_filter_one(backend, &ROOT_FILTER, fsentry_mask,
                                  statx_mask);
}

    /*--------------------------------------------------------------------*
     |                          filter fsentries                          |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
mongo_backend_filter_fsentries(void *backend, const struct rbh_filter *filter,
                               unsigned int fsentry_mask,
                               unsigned int statx_mask)
{
    struct mongo_backend *mongo = backend;
    struct mongo_iterator *mongo_iter;
    mongoc_cursor_t *cursor;
    bson_t *pipeline;

    if (rbh_filter_validate(filter))
        return NULL;

    pipeline = bson_pipeline_from_filter(filter);
    if (pipeline == NULL)
        return NULL;

    cursor = mongoc_collection_aggregate(mongo->entries, MONGOC_QUERY_NONE,
                                         pipeline, NULL, NULL);
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
    mongoc_database_destroy(mongo->db);
    mongoc_client_destroy(mongo->client);
    mongoc_uri_destroy(mongo->uri);
    free(mongo);
}

static const struct rbh_backend_operations MONGO_BACKEND_OPS = {
    .root = mongo_root,
    .update = mongo_backend_update,
    .filter_fsentries = mongo_backend_filter_fsentries,
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
