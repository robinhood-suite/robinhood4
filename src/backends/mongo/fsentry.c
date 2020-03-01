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
#include <sys/stat.h>

#include "robinhood/fsentry.h"
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

#include "mongo.h"

    /*--------------------------------------------------------------------*
     |                        fsentry_from_bson()                         |
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
            if (!bson_iter_statx_device(&subiter,
                                             &statxbuf->stx_rdev_major,
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

static void
_bson_iter_binary(bson_iter_t *iter, bson_subtype_t *subtype,
                  const char **data, size_t *size)
{
    uint32_t binary_len;

    bson_iter_binary(iter, subtype, &binary_len, (const uint8_t **)data);

    static_assert(SIZE_MAX >= UINT32_MAX, "");
    *size = binary_len;
}

static bool
bson_iter_rbh_id(bson_iter_t *iter, struct rbh_id *id)
{
    bson_subtype_t subtype;

    if (BSON_ITER_HOLDS_NULL(iter)) {
        id->size = 0;
        return true;
    }

    _bson_iter_binary(iter, &subtype, &id->data, &id->size);

    errno = EINVAL;
    return subtype == BSON_SUBTYPE_BINARY;
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

static bool
bson_iter_namespace(bson_iter_t *iter, struct rbh_fsentry *fsentry)
{
    while (bson_iter_next(iter)) {
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
        }
    }

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
bson_iter_fsentry(bson_iter_t *iter, struct rbh_fsentry *fsentry,
                  struct statx *statxbuf, const char **symlink)
{
    fsentry->mask = 0;
    *symlink = NULL;

    while (bson_iter_next(iter)) {
        bson_iter_t subiter;

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

            if (!bson_iter_namespace(&subiter, fsentry))
                return false;
            break;
        case FT_SYMLINK:
            if (!BSON_ITER_HOLDS_UTF8(iter))
                goto out_einval;

            *symlink = bson_iter_utf8(iter, NULL);
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
    } has = {
        .id = fsentry->mask & RBH_FP_ID,
        .parent = fsentry->mask & RBH_FP_PARENT_ID,
        .name = fsentry->mask & RBH_FP_NAME,
        .statx = fsentry->mask & RBH_FP_STATX,
    };

    return rbh_fsentry_new(has.id ? &fsentry->id : NULL,
                           has.parent ? &fsentry->parent_id : NULL,
                           has.name ? fsentry->name : NULL,
                           has.statx ? fsentry->statx : NULL, symlink);
}

struct rbh_fsentry *
fsentry_from_bson(const bson_t *bson)
{
    struct rbh_fsentry fsentry;
    struct statx statxbuf;
    const char *symlink;
    bson_iter_t iter;

    if (!bson_iter_init(&iter, bson)) {
        /* XXX: libbson is not quite clear on why this would happen, the code
         *      makes me think it only happens if `bson' is malformed.
         */
        errno = EINVAL;
        return NULL;
    }

    if (!bson_iter_fsentry(&iter, &fsentry, &statxbuf, &symlink))
        return NULL;

    return fsentry_almost_clone(&fsentry, symlink);
}
