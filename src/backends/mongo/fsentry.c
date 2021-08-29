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
#include <sys/stat.h>

#include "robinhood/fsentry.h"
#ifndef HAVE_STATX
# include "robinhood/statx-compat.h"
#endif

#include "mongo.h"
#include "utils.h"

/*----------------------------------------------------------------------------*
 |                            fsentry_from_bson()                             |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                         bson_iter_count()                          |
     *--------------------------------------------------------------------*/

static bool
bson_type_is_supported(bson_type_t type)
{
    switch (type) {
    case BSON_TYPE_UTF8:
    case BSON_TYPE_DOCUMENT:
    case BSON_TYPE_ARRAY:
    case BSON_TYPE_BINARY:
    case BSON_TYPE_BOOL:
    case BSON_TYPE_NULL:
    case BSON_TYPE_INT32:
    case BSON_TYPE_INT64:
        return true;
    case BSON_TYPE_REGEX:
        /* TODO: support it */
    default:
        return false;
    }
}

/* XXX: the iterator will be consumed, libbson lacks a practical
 *      bson_iter_copy() / bson_iter_tee() / bson_iter_dup()
 */
static size_t
bson_iter_count(bson_iter_t *iter)
{
    size_t count = 0;

    while (bson_iter_next(iter)) {
        if (bson_type_is_supported(bson_iter_type(iter)))
            count++;
    }

    return count;
}

    /*--------------------------------------------------------------------*
     |                     bson_iter_rbh_value_map()                      |
     *--------------------------------------------------------------------*/

static bool
bson_iter_rbh_value(bson_iter_t *iter, struct rbh_value *value,
                    char **buffer, size_t *bufsize);

static bool
bson_iter_rbh_value_map(bson_iter_t *iter, struct rbh_value_map *map,
                        size_t count, char **buffer, size_t *bufsize)
{
    struct rbh_value_pair *pairs;
    struct rbh_value *values;
    size_t size = *bufsize;
    char *data = *buffer;

    pairs = aligned_memalloc(alignof(*pairs), count * sizeof(*pairs), &data,
                             &size);
    if (pairs == NULL)
        return false;

    values = aligned_memalloc(alignof(*values), count * sizeof(*values), &data,
                              &size);
    if (values == NULL)
        return false;

    map->pairs = pairs;
    while (bson_iter_next(iter)) {
        if (!bson_iter_rbh_value(iter, values, &data, &size)) {
            if (errno == ENOTSUP)
                /* Ignore */
                continue;
            return false;
        }

        pairs->key = bson_iter_key(iter);
        pairs->value = values++;
        pairs++;
    }
    map->count = count;

    *buffer = data;
    *bufsize = size;
    return true;
}

    /*--------------------------------------------------------------------*
     |                       bson_iter_rbh_value()                        |
     *--------------------------------------------------------------------*/

static bool
bson_iter_rbh_value_sequence(bson_iter_t *iter, struct rbh_value *value,
                             size_t count, char **buffer, size_t *bufsize)
{
    struct rbh_value *values;
    size_t size = *bufsize;
    char *data = *buffer;

    value->type = RBH_VT_SEQUENCE;
    values = aligned_memalloc(alignof(*values), count * sizeof(*values), &data,
                              &size);
    if (values == NULL)
        return false;

    value->sequence.values = values;
    while (bson_iter_next(iter)) {
        if (!bson_iter_rbh_value(iter, values, &data, &size)) {
            if (errno == ENOTSUP)
                /* Ignore */
                continue;
            return false;
        }
        values++;
    }
    value->sequence.count = count;

    *buffer = data;
    *bufsize = size;
    return true;
}

static bool
bson_iter_rbh_value(bson_iter_t *iter, struct rbh_value *value,
                    char **buffer, size_t *bufsize)
{
    bson_iter_t subiter, tmp;
    const uint8_t *data;
    uint32_t size;

    switch (bson_iter_type(iter)) {
    case BSON_TYPE_UTF8:
        value->type = RBH_VT_STRING;
        value->string = bson_iter_utf8(iter, NULL);
        break;
    case BSON_TYPE_DOCUMENT:
        value->type = RBH_VT_MAP;
        bson_iter_recurse(iter, &tmp);
        bson_iter_recurse(iter, &subiter);
        if (!bson_iter_rbh_value_map(&subiter, &value->map,
                                     bson_iter_count(&tmp), buffer, bufsize))
            return false;
        break;
    case BSON_TYPE_ARRAY:
        value->type = RBH_VT_SEQUENCE;
        bson_iter_recurse(iter, &tmp);
        bson_iter_recurse(iter, &subiter);
        /* We cannot pass just `&value->sequence' as it is an unnamed struct */
        if (!bson_iter_rbh_value_sequence(&subiter, value,
                                          bson_iter_count(&tmp), buffer,
                                          bufsize))
            return false;
        break;
    case BSON_TYPE_BINARY:
        value->type = RBH_VT_BINARY;
        bson_iter_binary(iter, NULL, &size, &data);
        value->binary.data = (const char *)data;
        value->binary.size = size;
        break;
    case BSON_TYPE_BOOL:
        value->type = RBH_VT_INT32;
        value->int32 = bson_iter_bool(iter);
        break;
    case BSON_TYPE_NULL:
        value->type = RBH_VT_BINARY;
        value->binary.size = 0;
        break;
    case BSON_TYPE_REGEX:
        value->type = RBH_VT_REGEX;
        /* TODO: convert mongo regex options */
        errno = ENOTSUP;
        return false;
        break;
    case BSON_TYPE_INT32:
        value->type = RBH_VT_INT32;
        value->int32 = bson_iter_int32(iter);
        break;
    case BSON_TYPE_INT64:
        value->type = RBH_VT_INT64;
        value->int64 = bson_iter_int64(iter);
        break;
    default:
        errno = ENOTSUP;
        return false;
    }
    return true;
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
bson_iter_statx_attributes(bson_iter_t *iter, uint64_t *mask,
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
bson_iter_statx_timestamp(bson_iter_t *iter, struct statx_timestamp *timestamp)
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
bson_iter_statx_device(bson_iter_t *iter, uint32_t *major, uint32_t *minor)
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
bson_iter_statx(bson_iter_t *iter, struct statx *statxbuf)
{
    struct { /* mandatory fields */
        bool blksize:1;
        bool rdev:1;
        bool dev:1;
    } seen = {};

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
            if (!bson_iter_statx_attributes(
                        &subiter, (uint64_t *)&statxbuf->stx_attributes_mask,
                        (uint64_t *)&statxbuf->stx_attributes
                        ))
                return false;
            break;
        case ST_ATIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_timestamp(&subiter, &statxbuf->stx_atime))
                return false;
            statxbuf->stx_mask |= STATX_ATIME;
            break;
        case ST_BTIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_timestamp(&subiter, &statxbuf->stx_btime))
                return false;
            statxbuf->stx_mask |= STATX_BTIME;
            break;
        case ST_CTIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_timestamp(&subiter, &statxbuf->stx_ctime))
                return false;
            statxbuf->stx_mask |= STATX_CTIME;
            break;
        case ST_MTIME:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_timestamp(&subiter, &statxbuf->stx_mtime))
                return false;
            statxbuf->stx_mask |= STATX_MTIME;
            break;
        case ST_RDEV:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_device(&subiter, &statxbuf->stx_rdev_major,
                                        &statxbuf->stx_rdev_minor))
                return false;
            seen.rdev = true;
            break;
        case ST_DEV:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);
            if (!bson_iter_statx_device(&subiter, &statxbuf->stx_dev_major,
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

    /*--------------------------------------------------------------------*
     |                         bson_iter_rbh_id()                         |
     *--------------------------------------------------------------------*/

static void
_bson_iter_binary(bson_iter_t *iter, bson_subtype_t *subtype,
                  const char **data, size_t *size)
{
    uint32_t binary_len;

    if (BSON_ITER_HOLDS_NULL(iter)) {
        *subtype = BSON_SUBTYPE_BINARY;
        *size = 0;
        return;
    }

    bson_iter_binary(iter, subtype, &binary_len, (const uint8_t **)data);

    static_assert(SIZE_MAX >= UINT32_MAX, "");
    *size = binary_len;
}

static bool
bson_iter_rbh_id(bson_iter_t *iter, struct rbh_id *id)
{
    bson_subtype_t subtype;

    _bson_iter_binary(iter, &subtype, &id->data, &id->size);
    if (subtype != BSON_SUBTYPE_BINARY) {
        errno = EINVAL;
        return false;
    }
    return true;
}

enum namespace_token {
    NT_UNKNOWN,
    NT_PARENT,
    NT_NAME,
    NT_XATTRS,
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
    case 'x': /* xattrs */
        if (strcmp(key, "attrs"))
            break;
        return NT_XATTRS;
    }
    return NT_UNKNOWN;
}

static bool
bson_iter_namespace(bson_iter_t *iter, struct rbh_fsentry *fsentry,
                    char **buffer, size_t *bufsize)
{
    size_t size = *bufsize;
    char *data = *buffer;

    while (bson_iter_next(iter)) {
        bson_iter_t subiter;
        bson_iter_t tmp;

        switch (namespace_tokenizer(bson_iter_key(iter))) {
        case NT_UNKNOWN:
            break;
        case NT_PARENT:
            if (!BSON_ITER_HOLDS_NULL(iter) && !BSON_ITER_HOLDS_BINARY(iter))
                goto out_einval;

            if (!bson_iter_rbh_id(iter, &fsentry->parent_id))
                return false;
            fsentry->mask |= RBH_FP_PARENT_ID;
            break;
        case NT_NAME:
            if (!BSON_ITER_HOLDS_UTF8(iter))
                goto out_einval;

            fsentry->name = bson_iter_utf8(iter, NULL);
            fsentry->mask |= RBH_FP_NAME;
            break;
        case NT_XATTRS:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);

            bson_iter_recurse(iter, &tmp);
            if (!bson_iter_rbh_value_map(&subiter, &fsentry->xattrs.ns,
                                         bson_iter_count(&tmp), &data, &size))
                return false;
            fsentry->mask |= RBH_FP_NAMESPACE_XATTRS;
            break;
        }
    }

    *buffer = data;
    *bufsize = size;
    return true;

out_einval:
    errno = EINVAL;
    return false;
}

enum fsentry_token {
    FT_UNKNOWN,
    FT_ID,
    FT_NAMESPACE,
    FT_SYMLINK,
    FT_XATTRS,
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
    case 'x': /* xattrs */
        if (strcmp(key, "attrs"))
            break;
        return FT_XATTRS;
    }
    return FT_UNKNOWN;
}

static bool
bson_iter_fsentry(bson_iter_t *iter, struct rbh_fsentry *fsentry,
                  struct statx *statxbuf, const char **symlink, char **buffer,
                  size_t *bufsize)
{
    size_t size = *bufsize;
    char *data = *buffer;

    fsentry->mask = 0;
    *symlink = NULL;

    while (bson_iter_next(iter)) {
        bson_iter_t subiter;
        bson_iter_t tmp;

        switch (fsentry_tokenizer(bson_iter_key(iter))) {
        case FT_UNKNOWN:
            break;
        case FT_ID:
            if (!BSON_ITER_HOLDS_BINARY(iter))
                goto out_einval;

            if (!bson_iter_rbh_id(iter, &fsentry->id))
                return false;
            fsentry->mask |= RBH_FP_ID;
            break;
        case FT_NAMESPACE:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);

            if (!bson_iter_namespace(&subiter, fsentry, &data, &size))
                return false;
            break;
        case FT_SYMLINK:
            if (!BSON_ITER_HOLDS_UTF8(iter))
                goto out_einval;

            *symlink = bson_iter_utf8(iter, NULL);
            break;
        case FT_XATTRS:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);

            bson_iter_recurse(iter, &tmp);
            if (!bson_iter_rbh_value_map(&subiter, &fsentry->xattrs.inode,
                                         bson_iter_count(&tmp), &data, &size))
                return false;
            fsentry->mask |= RBH_FP_INODE_XATTRS;
            break;
        case FT_STATX:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);

            if (!bson_iter_statx(&subiter, statxbuf))
                return false;
            fsentry->statx = statxbuf;
            fsentry->mask |= RBH_FP_STATX;
            break;
        }
    }

    *bufsize = size;
    *buffer = data;
    return true;

out_einval:
    errno = EINVAL;
    return false;
}

static struct rbh_fsentry *
fsentry_almost_clone(const struct rbh_fsentry *fsentry, const char *symlink)
{
    struct {
        bool id:1;
        bool parent:1;
        bool name:1;
        bool statx:1;
        bool namespace_xattrs:1;
        bool inode_xattrs:1;
    } has = {
        .id = fsentry->mask & RBH_FP_ID,
        .parent = fsentry->mask & RBH_FP_PARENT_ID,
        .name = fsentry->mask & RBH_FP_NAME,
        .statx = fsentry->mask & RBH_FP_STATX,
        .namespace_xattrs = fsentry->mask & RBH_FP_NAMESPACE_XATTRS,
        .inode_xattrs = fsentry->mask & RBH_FP_INODE_XATTRS,
    };

    return rbh_fsentry_new(has.id ? &fsentry->id : NULL,
                           has.parent ? &fsentry->parent_id : NULL,
                           has.name ? fsentry->name : NULL,
                           has.statx ? fsentry->statx : NULL,
                           has.namespace_xattrs ? &fsentry->xattrs.ns : NULL,
                           has.inode_xattrs ? &fsentry->xattrs.inode : NULL,
                           symlink);
}

struct rbh_fsentry *
fsentry_from_bson(const bson_t *bson)
{
    struct rbh_fsentry fsentry;
    struct statx statxbuf;
    const char *symlink;
    bson_iter_t iter;
    char tmp[512]; /* TODO: figure out a better size than the arbitrary 512 */
    size_t bufsize = sizeof(tmp);
    char *buffer = tmp;

    if (!bson_iter_init(&iter, bson)) {
        /* XXX: libbson is not quite clear on why this would happen, the code
         *      makes me think it only happens if `bson' is malformed.
         */
        errno = EINVAL;
        return NULL;
    }

    if (!bson_iter_fsentry(&iter, &fsentry, &statxbuf, &symlink, &buffer,
                           &bufsize))
        /* FIXME: while it is nice to try to parse most fsentries without
         *        allocating any memory using the "on stack" buffer `tmp', there
         *        should be a fallback for when that fails.
         */
        return NULL;

    return fsentry_almost_clone(&fsentry, symlink);
}
