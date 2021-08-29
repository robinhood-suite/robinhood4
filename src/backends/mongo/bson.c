/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/stat.h>

#include "robinhood/value.h"
#ifndef HAVE_STATX
# include "robinhood/statx-compat.h"
#endif

#include "mongo.h"

static bool
bson_append_statx_attributes(bson_t *bson, const char *key, size_t key_length,
                             uint64_t mask, uint64_t attributes)
{
    bson_t document;

    return bson_append_document_begin(bson, key, key_length, &document)
        && (mask & STATX_ATTR_COMPRESSED ?
                BSON_APPEND_BOOL(&document, MFF_STATX_COMPRESSED,
                                 attributes & STATX_ATTR_COMPRESSED) : true)
        && (mask & STATX_ATTR_IMMUTABLE ?
                BSON_APPEND_BOOL(&document, MFF_STATX_IMMUTABLE,
                                 attributes & STATX_ATTR_IMMUTABLE) : true)
        && (mask & STATX_ATTR_APPEND ?
                BSON_APPEND_BOOL(&document, MFF_STATX_APPEND,
                                 attributes & STATX_ATTR_APPEND) : true)
        && (mask & STATX_ATTR_NODUMP ?
                BSON_APPEND_BOOL(&document, MFF_STATX_NODUMP,
                                 attributes & STATX_ATTR_NODUMP) : true)
        && (mask & STATX_ATTR_ENCRYPTED ?
                BSON_APPEND_BOOL(&document, MFF_STATX_ENCRYPTED,
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
        && BSON_APPEND_INT64(&document, MFF_STATX_TIMESTAMP_SEC,
                             timestamp->tv_sec)
        && BSON_APPEND_INT32(&document, MFF_STATX_TIMESTAMP_NSEC,
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
        && BSON_APPEND_INT32(&document, MFF_STATX_DEVICE_MAJOR, major)
        && BSON_APPEND_INT32(&document, MFF_STATX_DEVICE_MINOR, minor)
        && bson_append_document_end(bson, &document);
}

#define BSON_APPEND_STATX_DEVICE(bson, key, major, minor) \
    bson_append_statx_device(bson, key, strlen(key), major, minor)

bool
bson_append_statx(bson_t *bson, const char *key, size_t key_length,
                  const struct statx *statxbuf)
{
    bson_t document;

    return bson_append_document_begin(bson, key, key_length, &document)
        && BSON_APPEND_INT32(&document, MFF_STATX_BLKSIZE,
                             statxbuf->stx_blksize)
        && (statxbuf->stx_mask & STATX_NLINK ?
                BSON_APPEND_INT32(&document, MFF_STATX_NLINK,
                                  statxbuf->stx_nlink) : true)
        && (statxbuf->stx_mask & STATX_UID ?
                BSON_APPEND_INT32(&document, MFF_STATX_UID, statxbuf->stx_uid) :
                true)
        && (statxbuf->stx_mask & STATX_GID ?
                BSON_APPEND_INT32(&document, MFF_STATX_GID, statxbuf->stx_gid) :
                true)
        && (statxbuf->stx_mask & STATX_TYPE ?
                BSON_APPEND_INT32(&document, MFF_STATX_TYPE,
                                  statxbuf->stx_mode & S_IFMT) : true)
        && (statxbuf->stx_mask & STATX_MODE ?
                BSON_APPEND_INT32(&document, MFF_STATX_MODE,
                                  statxbuf->stx_mode & ~S_IFMT) : true)
        && (statxbuf->stx_mask & STATX_INO ?
                BSON_APPEND_INT64(&document, MFF_STATX_INO, statxbuf->stx_ino) :
                true)
        && (statxbuf->stx_mask & STATX_SIZE ?
                BSON_APPEND_INT64(&document, MFF_STATX_SIZE,
                                  statxbuf->stx_size) : true)
        && (statxbuf->stx_mask & STATX_BLOCKS ?
                BSON_APPEND_INT64(&document, MFF_STATX_BLOCKS,
                                  statxbuf->stx_blocks) : true)
        && BSON_APPEND_STATX_ATTRIBUTES(&document,
                                        MFF_STATX_ATTRIBUTES,
                                        statxbuf->stx_attributes_mask,
                                        statxbuf->stx_attributes)
        && (statxbuf->stx_mask & STATX_ATIME ?
                BSON_APPEND_STATX_TIMESTAMP(&document, MFF_STATX_ATIME,
                                            &statxbuf->stx_atime) : true)
        && (statxbuf->stx_mask & STATX_BTIME ?
                BSON_APPEND_STATX_TIMESTAMP(&document, MFF_STATX_BTIME,
                                            &statxbuf->stx_btime) : true)
        && (statxbuf->stx_mask & STATX_CTIME ?
                BSON_APPEND_STATX_TIMESTAMP(&document, MFF_STATX_CTIME,
                                            &statxbuf->stx_ctime) : true)
        && (statxbuf->stx_mask & STATX_MTIME ?
                BSON_APPEND_STATX_TIMESTAMP(&document, MFF_STATX_MTIME,
                                            &statxbuf->stx_mtime) : true)
        && BSON_APPEND_STATX_DEVICE(&document, MFF_STATX_RDEV,
                                    statxbuf->stx_rdev_major,
                                    statxbuf->stx_rdev_minor)
        && BSON_APPEND_STATX_DEVICE(&document, MFF_STATX_DEV,
                                    statxbuf->stx_dev_major,
                                    statxbuf->stx_dev_minor)
        && bson_append_document_end(bson, &document);
}

/*----------------------------------------------------------------------------*
 |                          bson_append_setxattrs()                           |
 *----------------------------------------------------------------------------*/

#define ONSTACK_KEYLEN_MAX 128

static bool
bson_append_xattr(bson_t *bson, const char *prefix, const char *xattr,
                  const struct rbh_value *value)
{
    char onstack[ONSTACK_KEYLEN_MAX];
    char *key = onstack;
    bool success;
    int keylen;

    keylen = snprintf(key, sizeof(onstack), "%s.%s", prefix, xattr);
    if (keylen >= sizeof(onstack))
        keylen = asprintf(&key, "%s.%s", prefix, xattr);
    if (keylen < 0)
        return false;

    if (value == NULL)
        success = bson_append_null(bson, key, keylen);
    else
        success = bson_append_rbh_value(bson, key, keylen, value);
    if (key != onstack)
        free(key);

    errno = ENOBUFS;
    return success;
}

bool
bson_append_setxattrs(bson_t *bson, const char *prefix,
                      const struct rbh_value_map *xattrs)
{
    for (size_t i = 0; i < xattrs->count; i++) {
        const struct rbh_value *value = xattrs->pairs[i].value;
        const char *xattr = xattrs->pairs[i].key;

        /* Skip xattrs that are to be unset */
        if (value == NULL)
            continue;

        if (!bson_append_xattr(bson, prefix, xattr, value))
            return false;
    }

    return true;
}

/*----------------------------------------------------------------------------*
 |                         bson_append_unsetxattrs()                          |
 *----------------------------------------------------------------------------*/

bool
bson_append_unsetxattrs(bson_t *bson, const char *prefix,
                        const struct rbh_value_map *xattrs)
{
    for (size_t i = 0; i < xattrs->count; i++) {
        const struct rbh_value *value = xattrs->pairs[i].value;
        const char *xattr = xattrs->pairs[i].key;

        /* Skip xattrs that are to be set */
        if (value)
            continue;

        if (!bson_append_xattr(bson, prefix, xattr, NULL))
            return false;
    }

    return true;
}
