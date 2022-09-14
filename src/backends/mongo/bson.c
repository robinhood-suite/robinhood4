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
#include "robinhood/statx.h"

#include "mongo.h"

static bool
bson_append_statx_attributes(bson_t *bson, const char *key, size_t key_length,
                             uint64_t mask, uint64_t attributes)
{
    bson_t document = *bson;

    return (mask & RBH_STATX_ATTR_COMPRESSED ?
                BSON_APPEND_BOOL(&document, attr2str(RBH_STATX_ATTR_COMPRESSED),
                                 attributes & RBH_STATX_ATTR_COMPRESSED) : true)
        && (mask & RBH_STATX_ATTR_IMMUTABLE ?
                BSON_APPEND_BOOL(&document, attr2str(RBH_STATX_ATTR_IMMUTABLE),
                                 attributes & RBH_STATX_ATTR_IMMUTABLE) : true)
        && (mask & RBH_STATX_ATTR_APPEND ?
                BSON_APPEND_BOOL(&document, attr2str(RBH_STATX_ATTR_APPEND),
                                 attributes & RBH_STATX_ATTR_APPEND) : true)
        && (mask & RBH_STATX_ATTR_NODUMP ?
                BSON_APPEND_BOOL(&document, attr2str(RBH_STATX_ATTR_NODUMP),
                                 attributes & RBH_STATX_ATTR_NODUMP) : true)
        && (mask & RBH_STATX_ATTR_ENCRYPTED ?
                BSON_APPEND_BOOL(&document, attr2str(RBH_STATX_ATTR_ENCRYPTED),
                                 attributes & RBH_STATX_ATTR_ENCRYPTED) : true)
        && (mask & RBH_STATX_ATTR_AUTOMOUNT ?
                BSON_APPEND_BOOL(&document, attr2str(RBH_STATX_ATTR_AUTOMOUNT),
                                 attributes & RBH_STATX_ATTR_AUTOMOUNT) : true)
        && (mask & RBH_STATX_ATTR_MOUNT_ROOT ?
                BSON_APPEND_BOOL(&document, attr2str(RBH_STATX_ATTR_MOUNT_ROOT),
                                 attributes & RBH_STATX_ATTR_MOUNT_ROOT) : true)
        && (mask & RBH_STATX_ATTR_VERITY ?
                BSON_APPEND_BOOL(&document, attr2str(RBH_STATX_ATTR_VERITY),
                                 attributes & RBH_STATX_ATTR_VERITY) : true)
        && (mask & RBH_STATX_ATTR_DAX ?
                BSON_APPEND_BOOL(&document, attr2str(RBH_STATX_ATTR_DAX),
                                 attributes & RBH_STATX_ATTR_DAX) : true);
}

#define BSON_APPEND_STATX_ATTRIBUTES(bson, key, mask, attributes) \
    bson_append_statx_attributes(bson, key, strlen(key), mask, attributes)

bool
bson_append_statx(bson_t *bson, const char *key, size_t key_length,
                  const struct rbh_statx *statxbuf)
{
    bson_t *document = bson;
    bson_t subdoc;

    return (statxbuf->stx_mask & RBH_STATX_BLKSIZE ?
                BSON_APPEND_INT32(document, statx2str(RBH_STATX_BLKSIZE),
                                  statxbuf->stx_blksize) : true)
        && (statxbuf->stx_mask & RBH_STATX_NLINK ?
                BSON_APPEND_INT32(document, statx2str(RBH_STATX_NLINK),
                                  statxbuf->stx_nlink) : true)
        && (statxbuf->stx_mask & RBH_STATX_UID ?
                BSON_APPEND_INT32(document, statx2str(RBH_STATX_UID),
                                  statxbuf->stx_uid) : true)
        && (statxbuf->stx_mask & RBH_STATX_GID ?
                BSON_APPEND_INT32(document, statx2str(RBH_STATX_GID),
                                  statxbuf->stx_gid) : true)
        && (statxbuf->stx_mask & RBH_STATX_TYPE ?
                BSON_APPEND_INT32(document, statx2str(RBH_STATX_TYPE),
                                  statxbuf->stx_mode & S_IFMT) : true)
        && (statxbuf->stx_mask & RBH_STATX_MODE ?
                BSON_APPEND_INT32(document, statx2str(RBH_STATX_MODE),
                                  statxbuf->stx_mode & ~S_IFMT) : true)
        && (statxbuf->stx_mask & RBH_STATX_INO ?
                BSON_APPEND_INT64(document, statx2str(RBH_STATX_INO),
                                  statxbuf->stx_ino) : true)
        && (statxbuf->stx_mask & RBH_STATX_SIZE ?
                BSON_APPEND_INT64(document, statx2str(RBH_STATX_SIZE),
                                  statxbuf->stx_size) : true)
        && (statxbuf->stx_mask & RBH_STATX_BLOCKS ?
                BSON_APPEND_INT64(document, statx2str(RBH_STATX_BLOCKS),
                                  statxbuf->stx_blocks) : true)
        && (statxbuf->stx_mask & RBH_STATX_ATTRIBUTES ?
                BSON_APPEND_STATX_ATTRIBUTES(document,
                                             statx2str(RBH_STATX_ATTRIBUTES),
                                             statxbuf->stx_attributes_mask,
                                             statxbuf->stx_attributes) : true)
        && (statxbuf->stx_mask & RBH_STATX_ATIME ?
                BSON_APPEND_DOCUMENT_BEGIN(document,
                                           subdoc2str(RBH_STATX_ATIME),
                                           &subdoc)
             && (statxbuf->stx_mask & RBH_STATX_ATIME_SEC ?
                     BSON_APPEND_INT64(&subdoc, MFF_STATX_TIMESTAMP_SEC,
                                       statxbuf->stx_atime.tv_sec) : true)
             && (statxbuf->stx_mask & RBH_STATX_ATIME_NSEC ?
                     BSON_APPEND_INT32(&subdoc, MFF_STATX_TIMESTAMP_NSEC,
                                       statxbuf->stx_atime.tv_nsec) : true)
             && bson_append_document_end(document, &subdoc)
              : true)
        && (statxbuf->stx_mask & RBH_STATX_BTIME ?
                BSON_APPEND_DOCUMENT_BEGIN(document,
                                           subdoc2str(RBH_STATX_BTIME),
                                           &subdoc)
             && (statxbuf->stx_mask & RBH_STATX_BTIME_SEC ?
                     BSON_APPEND_INT64(&subdoc, MFF_STATX_TIMESTAMP_SEC,
                                       statxbuf->stx_btime.tv_sec) : true)
             && (statxbuf->stx_mask & RBH_STATX_BTIME_NSEC ?
                     BSON_APPEND_INT32(&subdoc, MFF_STATX_TIMESTAMP_NSEC,
                                       statxbuf->stx_btime.tv_nsec) : true)
             && bson_append_document_end(document, &subdoc)
              : true)
        && (statxbuf->stx_mask & RBH_STATX_CTIME ?
                BSON_APPEND_DOCUMENT_BEGIN(document,
                                           subdoc2str(RBH_STATX_CTIME),
                                           &subdoc)
             && (statxbuf->stx_mask & RBH_STATX_CTIME_SEC ?
                     BSON_APPEND_INT64(&subdoc, MFF_STATX_TIMESTAMP_SEC,
                                       statxbuf->stx_ctime.tv_sec) : true)
             && (statxbuf->stx_mask & RBH_STATX_CTIME_NSEC ?
                     BSON_APPEND_INT32(&subdoc, MFF_STATX_TIMESTAMP_NSEC,
                                       statxbuf->stx_ctime.tv_nsec) : true)
             && bson_append_document_end(document, &subdoc)
              : true)
        && (statxbuf->stx_mask & RBH_STATX_MTIME ?
                BSON_APPEND_DOCUMENT_BEGIN(document,
                                           subdoc2str(RBH_STATX_MTIME),
                                           &subdoc)
             && (statxbuf->stx_mask & RBH_STATX_MTIME_SEC ?
                     BSON_APPEND_INT64(&subdoc, MFF_STATX_TIMESTAMP_SEC,
                                       statxbuf->stx_mtime.tv_sec) : true)
             && (statxbuf->stx_mask & RBH_STATX_MTIME_NSEC ?
                     BSON_APPEND_INT32(&subdoc, MFF_STATX_TIMESTAMP_NSEC,
                                       statxbuf->stx_mtime.tv_nsec) : true)
             && bson_append_document_end(document, &subdoc)
              : true)
        && (statxbuf->stx_mask & RBH_STATX_RDEV ?
                BSON_APPEND_DOCUMENT_BEGIN(document,
                                           subdoc2str(RBH_STATX_RDEV),
                                           &subdoc)
             && (statxbuf->stx_mask & RBH_STATX_RDEV_MAJOR ?
                     BSON_APPEND_INT32(&subdoc, MFF_STATX_DEVICE_MAJOR,
                                       statxbuf->stx_rdev_major) : true)
             && (statxbuf->stx_mask & RBH_STATX_RDEV_MINOR ?
                     BSON_APPEND_INT32(&subdoc, MFF_STATX_DEVICE_MINOR,
                                       statxbuf->stx_rdev_minor) : true)
             && bson_append_document_end(document, &subdoc)
              : true)
        && (statxbuf->stx_mask & RBH_STATX_DEV ?
                BSON_APPEND_DOCUMENT_BEGIN(document,
                                           subdoc2str(RBH_STATX_DEV),
                                           &subdoc)
             && (statxbuf->stx_mask & RBH_STATX_DEV_MAJOR ?
                     BSON_APPEND_INT32(&subdoc, MFF_STATX_DEVICE_MAJOR,
                                       statxbuf->stx_dev_major) : true)
             && (statxbuf->stx_mask & RBH_STATX_DEV_MINOR ?
                     BSON_APPEND_INT32(&subdoc, MFF_STATX_DEVICE_MINOR,
                                       statxbuf->stx_dev_minor) : true)
             && bson_append_document_end(document, &subdoc)
              : true)
        && (statxbuf->stx_mask & RBH_STATX_MNT_ID ?
                BSON_APPEND_INT64(document, statx2str(RBH_STATX_MNT_ID),
                                  statxbuf->stx_mnt_id) : true);
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
