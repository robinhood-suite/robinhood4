/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <sys/stat.h>

#include "robinhood/value.h"
#include "robinhood/statx.h"

#include "mongo.h"

static bool
bson_append_statx_attributes(bson_t *bson, const char *key, size_t key_length,
                             uint64_t mask, uint64_t attributes)
{
    bson_t document = *bson;

    (void) key;
    (void) key_length;

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

    (void) key;
    (void) key_length;

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

    /*--------------------------------------------------------------------*
     |                         bson_iter_statx()                          |
     *--------------------------------------------------------------------*/

enum statx_attributes_token {
    SAT_UNKNOWN,
    SAT_COMPRESSED,
    SAT_IMMUTABLE,
    SAT_APPEND,
    SAT_NODUMP,
    SAT_ENCRYPTED,
    SAT_AUTOMOUNT,
    SAT_MOUNT_ROOT,
    SAT_VERITY,
    SAT_DAX,
};

static enum statx_attributes_token
statx_attributes_tokenizer(const char *key)
{
    switch (*key++) {
    case 'a': /* append, automount */
        switch (*key++) {
        case 'p': /* append */
            if (strcmp(key, "pend"))
                break;
            return SAT_APPEND;
        case 'u': /* automount */
            if (strcmp(key, "tomount"))
                break;
            return SAT_AUTOMOUNT;
        }
        break;
    case 'c': /* compressed */
        if (strcmp(key, "ompressed"))
            break;
        return SAT_COMPRESSED;
    case 'd': /* dax */
        if (strcmp(key, "ax"))
            break;
        return SAT_DAX;
    case 'e': /* encrypted */
        if (strcmp(key, "ncrypted"))
            break;
        return SAT_ENCRYPTED;
    case 'i': /* immutable */
        if (strcmp(key, "mmutable"))
            break;
        return SAT_IMMUTABLE;
    case 'm': /* mount-root */
        if (strcmp(key, "ount-root"))
            break;
        return SAT_MOUNT_ROOT;
    case 'n': /* nodump */
        if (strcmp(key, "odump"))
            break;
        return SAT_NODUMP;
    case 'v': /* verity */
        if (strcmp(key, "erity"))
            break;
        return SAT_VERITY;
    }
    return SAT_UNKNOWN;
}

static bool
bson_iter_statx_attributes(bson_iter_t *iter, uint64_t *mask,
                           uint64_t *attributes)
{
    *attributes = 0;
    *mask = 0;

    while (bson_iter_next(iter)) {
        switch (statx_attributes_tokenizer(bson_iter_key(iter))) {
        case SAT_UNKNOWN:
            break;
        case SAT_COMPRESSED:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= RBH_STATX_ATTR_COMPRESSED;
            else
                *attributes &= ~RBH_STATX_ATTR_COMPRESSED;
            *mask |= RBH_STATX_ATTR_COMPRESSED;
            break;
        case SAT_IMMUTABLE:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= RBH_STATX_ATTR_IMMUTABLE;
            else
                *attributes &= ~RBH_STATX_ATTR_IMMUTABLE;
            *mask |= RBH_STATX_ATTR_IMMUTABLE;
            break;
        case SAT_APPEND:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= RBH_STATX_ATTR_APPEND;
            else
                *attributes &= ~RBH_STATX_ATTR_APPEND;
            *mask |= RBH_STATX_ATTR_APPEND;
            break;
        case SAT_NODUMP:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= RBH_STATX_ATTR_NODUMP;
            else
                *attributes &= ~RBH_STATX_ATTR_NODUMP;
            *mask |= RBH_STATX_ATTR_NODUMP;
            break;
        case SAT_ENCRYPTED:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= RBH_STATX_ATTR_ENCRYPTED;
            else
                *attributes &= ~RBH_STATX_ATTR_ENCRYPTED;
            *mask |= RBH_STATX_ATTR_ENCRYPTED;
            break;
        case SAT_AUTOMOUNT:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= RBH_STATX_ATTR_AUTOMOUNT;
            else
                *attributes &= ~RBH_STATX_ATTR_AUTOMOUNT;
            *mask |= RBH_STATX_ATTR_AUTOMOUNT;
            break;
        case SAT_MOUNT_ROOT:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= RBH_STATX_ATTR_MOUNT_ROOT;
            else
                *attributes &= ~RBH_STATX_ATTR_MOUNT_ROOT;
            *mask |= RBH_STATX_ATTR_MOUNT_ROOT;
            break;
        case SAT_VERITY:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= RBH_STATX_ATTR_VERITY;
            else
                *attributes &= ~RBH_STATX_ATTR_VERITY;
            *mask |= RBH_STATX_ATTR_VERITY;
            break;
        case SAT_DAX:
            if (!BSON_ITER_HOLDS_BOOL(iter))
                goto out_einval;
            if (bson_iter_bool(iter))
                *attributes |= RBH_STATX_ATTR_DAX;
            else
                *attributes &= ~RBH_STATX_ATTR_DAX;
            *mask |= RBH_STATX_ATTR_DAX;
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
bson_iter_statx_timestamp(bson_iter_t *iter, uint32_t *mask,
                          uint32_t tv_sec_flag, uint32_t tv_nsec_flag,
                          struct rbh_statx_timestamp *timestamp)
{
    while (bson_iter_next(iter)) {
        switch (statx_timestamp_tokenizer(bson_iter_key(iter))) {
        case STT_UNKNOWN:
            break;
        case STT_SEC:
            if (!BSON_ITER_HOLDS_INT64(iter))
                goto out_einval;
            timestamp->tv_sec = bson_iter_int64(iter);
            *mask |= tv_sec_flag;
            break;
        case STT_NSEC:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            timestamp->tv_nsec = bson_iter_int32(iter);
            *mask |= tv_nsec_flag;
            break;
        }
    }

    return true;

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
bson_iter_statx_device(bson_iter_t *iter, uint32_t *mask, uint32_t major_flag,
                       uint32_t *major, uint32_t minor_flag, uint32_t *minor)
{
    while (bson_iter_next(iter)) {
        switch (statx_device_tokenizer(bson_iter_key(iter))) {
        case SDT_UNKNOWN:
            break;
        case SDT_MAJOR:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            *major = bson_iter_int32(iter);
            *mask |= major_flag;
            break;
        case SDT_MINOR:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            *minor = bson_iter_int32(iter);
            *mask |= minor_flag;
            break;
        }
    }

    return true;

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
    ST_MNT_ID,
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
    case 'm': /* mode, mount-id, mtime */
        switch (*key++) {
        case 'o': /* mode, mount-id */
            switch (*key++) {
            case 'd': /* mode */
                if (strcmp(key, "e"))
                    break;
                return ST_MODE;
            case 'u': /* mount-id */
                if (strcmp(key, "nt-id"))
                    break;
                return ST_MNT_ID;
            }
            break;
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

bool
bson_iter_statx(bson_iter_t *iter, struct rbh_statx *statxbuf)
{
    statxbuf->stx_mask = 0;
    statxbuf->stx_mode = 0;

    while (bson_iter_next(iter)) {
        bson_iter_t subiter;

        switch (statx_tokenizer(bson_iter_key(iter))) {
        case ST_UNKNOWN:
            break;
        case ST_BLKSIZE:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_blksize = bson_iter_int32(iter);
            statxbuf->stx_mask |= RBH_STATX_BLKSIZE;
            break;
        case ST_NLINK:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_nlink = bson_iter_int32(iter);
            statxbuf->stx_mask |= RBH_STATX_NLINK;
            break;
        case ST_UID:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_uid = bson_iter_int32(iter);
            statxbuf->stx_mask |= RBH_STATX_UID;
            break;
        case ST_GID:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_gid = bson_iter_int32(iter);
            statxbuf->stx_mask |= RBH_STATX_GID;
            break;
        case ST_TYPE:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_mode |= bson_iter_int32(iter) & S_IFMT;
            statxbuf->stx_mask |= RBH_STATX_TYPE;
            break;
        case ST_MODE:
            if (!BSON_ITER_HOLDS_INT32(iter))
                goto out_einval;
            statxbuf->stx_mode |= bson_iter_int32(iter) & ~S_IFMT;
            statxbuf->stx_mask |= RBH_STATX_MODE;
            break;
        case ST_INO:
            if (!BSON_ITER_HOLDS_INT64(iter))
                goto out_einval;
            statxbuf->stx_ino = bson_iter_int64(iter);
            statxbuf->stx_mask |= RBH_STATX_INO;
            break;
        case ST_SIZE:
            if (!BSON_ITER_HOLDS_INT64(iter))
                goto out_einval;
            statxbuf->stx_size = bson_iter_int64(iter);
            statxbuf->stx_mask |= RBH_STATX_SIZE;
            break;
        case ST_BLOCKS:
            if (!BSON_ITER_HOLDS_INT64(iter))
                goto out_einval;
            statxbuf->stx_blocks = bson_iter_int64(iter);
            statxbuf->stx_mask |= RBH_STATX_BLOCKS;
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
            if (!bson_iter_statx_attributes(
                        &subiter, (uint64_t *)&statxbuf->stx_attributes_mask,
                        (uint64_t *)&statxbuf->stx_attributes
                        ))
                return false;
            statxbuf->stx_mask |= RBH_STATX_ATTRIBUTES;
            break;
        case ST_ATIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_timestamp(&subiter, &statxbuf->stx_mask,
                                           RBH_STATX_ATIME_SEC,
                                           RBH_STATX_ATIME_NSEC,
                                           &statxbuf->stx_atime))
                return false;
            break;
        case ST_BTIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_timestamp(&subiter, &statxbuf->stx_mask,
                                           RBH_STATX_BTIME_SEC,
                                           RBH_STATX_BTIME_NSEC,
                                           &statxbuf->stx_btime))
                return false;
            break;
        case ST_CTIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_timestamp(&subiter, &statxbuf->stx_mask,
                                           RBH_STATX_CTIME_SEC,
                                           RBH_STATX_CTIME_NSEC,
                                           &statxbuf->stx_ctime))
                return false;
            break;
        case ST_MTIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_timestamp(&subiter, &statxbuf->stx_mask,
                                           RBH_STATX_MTIME_SEC,
                                           RBH_STATX_MTIME_NSEC,
                                           &statxbuf->stx_mtime))
                return false;
            break;
        case ST_RDEV:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_device(&subiter, &statxbuf->stx_mask,
                                        RBH_STATX_RDEV_MAJOR,
                                        &statxbuf->stx_rdev_major,
                                        RBH_STATX_RDEV_MINOR,
                                        &statxbuf->stx_rdev_minor))
                return false;
            break;
        case ST_DEV:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_device(&subiter, &statxbuf->stx_mask,
                                        RBH_STATX_DEV_MAJOR,
                                        &statxbuf->stx_dev_major,
                                        RBH_STATX_DEV_MINOR,
                                        &statxbuf->stx_dev_minor))
                return false;
            break;
        case ST_MNT_ID:
            if (!BSON_ITER_HOLDS_INT64(iter))
                goto out_einval;
            statxbuf->stx_mnt_id = bson_iter_int64(iter);
            statxbuf->stx_mask |= RBH_STATX_MNT_ID;
            break;
        }
    }

    return true;

out_einval:
    errno = EINVAL;
    return false;
}
