/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_MONGO_H
#define RBH_MONGO_H

#include <stdbool.h>

#include <bson.h>

#include <robinhood/id.h>

struct rbh_filter;
struct rbh_fsentry;
struct rbh_fsevent;
struct rbh_value;
struct rbh_value_map;

/*----------------------------------------------------------------------------*
 |                            Mongo FSEntry layout                            |
 *----------------------------------------------------------------------------*/

/* Fentries are stored in mongo using the following layout:
 *
 * {
 *     _id: fsentry.id (BINARY, SUBTYPE_BINARY)
 *
 *     ns: [{
 *         parent: fsentry.parent_id (BINARY, SUBTYPE_BINARY)
 *         name: fsentry.name (UTF8)
 *         xattrs: {
 *             <key>: <value> (RBH_VALUE)
 *             ...
 *         }
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
 *
 *     xattrs: {
 *         <key>: <value> (RBH_VALUE)
 *         ...
 *     }
 * }
 *
 * Note that when they are fetched _from_ the database, the "ns" field is
 * unwinded so that we do not have to unwind it ourselves.
 */

    /*--------------------------------------------------------------------*
     |                        Mongo FSEntry Fields                        |
     *--------------------------------------------------------------------*/

/* ID */
#define MFF_ID                      "_id"

/* Namespace */
#define MFF_NAMESPACE               "ns"
#define MFF_PARENT_ID               "parent"
#define MFF_NAME                    "name"

/* xattrs (inode & namespace) */
#define MFF_XATTRS                  "xattrs"

/* symlink */
#define MFF_SYMLINK                 "symlink"

/* statx */
#define MFF_STATX                   "statx"
#define MFF_STATX_BLKSIZE           "blksize"
#define MFF_STATX_NLINK             "nlink"
#define MFF_STATX_UID               "uid"
#define MFF_STATX_GID               "gid"
#define MFF_STATX_TYPE              "type"
#define MFF_STATX_MODE              "mode"
#define MFF_STATX_INO               "ino"
#define MFF_STATX_SIZE              "size"
#define MFF_STATX_BLOCKS            "blocks"

/* statx->stx_attributes */
#define MFF_STATX_ATTRIBUTES        "attributes"
#define MFF_STATX_COMPRESSED        "compressed"
#define MFF_STATX_IMMUTABLE         "immutable"
#define MFF_STATX_APPEND            "append"
#define MFF_STATX_NODUMP            "nodump"
#define MFF_STATX_ENCRYPTED         "encrypted"

/* statx_timestamp */
#define MFF_STATX_TIMESTAMP_SEC     "sec"
#define MFF_STATX_TIMESTAMP_NSEC    "nsec"

#define MFF_STATX_ATIME             "atime"
#define MFF_STATX_BTIME             "btime"
#define MFF_STATX_CTIME             "ctime"
#define MFF_STATX_MTIME             "mtime"

/* "statx_device" */
#define MFF_STATX_DEVICE_MAJOR      "major"
#define MFF_STATX_DEVICE_MINOR      "minor"

#define MFF_STATX_RDEV              "rdev"
#define MFF_STATX_DEV               "dev"

/*----------------------------------------------------------------------------*
 |                                bson helpers                                |
 *----------------------------------------------------------------------------*/

static inline bool
_bson_append_binary(bson_t *bson, const char *key, size_t key_length,
                    bson_subtype_t subtype, const char *data, size_t size)
{
    return bson_append_binary(bson, key, key_length, subtype,
                              (const uint8_t *)data, size);
}

static inline bool
bson_append_rbh_id(bson_t *bson, const char *key, size_t key_length,
                   const struct rbh_id *id)
{
    return _bson_append_binary(bson, key, key_length, BSON_SUBTYPE_BINARY,
                               id->data, id->size);
}

#define BSON_APPEND_RBH_ID(bson, key, id) \
    bson_append_rbh_id(bson, key, strlen(key), id)

bool
bson_append_statx(bson_t *bson, const char *key, size_t key_length,
                  const struct statx *statxbuf);

#define BSON_APPEND_STATX(bson, key, statxbuf) \
    bson_append_statx(bson, key, strlen(key), statxbuf)

bool
bson_append_setxattrs(bson_t *bson, const char *prefix,
                      const struct rbh_value_map *xattrs);

bool
bson_append_unsetxattrs(bson_t *bson, const char *prefix,
                        const struct rbh_value_map *xattrs);

    /*--------------------------------------------------------------------*
     |                              fsentry                               |
     *--------------------------------------------------------------------*/

struct rbh_fsentry *
fsentry_from_bson(const bson_t *bson);

    /*--------------------------------------------------------------------*
     |                               filter                               |
     *--------------------------------------------------------------------*/

/* Should only be used on a valid filter */
bool
bson_append_rbh_filter(bson_t *bson, const char *key, size_t key_length,
                       const struct rbh_filter *filter, bool negate);

#define BSON_APPEND_RBH_FILTER(bson, key, filter) \
    bson_append_rbh_filter(bson, key, strlen(key), filter, false)

    /*--------------------------------------------------------------------*
     |                              fsevent                               |
     *--------------------------------------------------------------------*/

bson_t *
bson_update_from_fsevent(const struct rbh_fsevent *fsevent);

    /*--------------------------------------------------------------------*
     |                               value                                |
     *--------------------------------------------------------------------*/

bool
bson_append_rbh_value(bson_t *bson, const char *key, size_t key_length,
                      const struct rbh_value *value);

#define BSON_APPEND_RBH_VALUE(bson, key, value) \
    bson_append_rbh_value(bson, key, strlen(key), value)

bool
bson_append_rbh_value_map(bson_t *bson, const char *key, size_t key_length,
                          const struct rbh_value_map *map);

#define BSON_APPEND_RBH_VALUE_MAP(bson, key, map) \
    bson_append_rbh_value_map(bson, key, strlen(key), map)

#endif
