/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

#include <sys/stat.h>
#include <robinhood/statx.h>

#define foreach_bit_set(mask, index)  \
    for (index = ffs(mask);           \
         mask;                        \
         mask &= ~(1 << (index - 1)), \
         index = ffs(mask))

#define bit2mask(n) (1 << (n - 1))

enum entries_attribute {
    EA_BLKSIZE,
    EA_NLINK,
    EA_UID,
    EA_GID,
    EA_MODE,
    EA_TYPE,
    EA_INO,
    EA_SIZE,
    EA_BLOCKS,
    EA_ATIME_SEC,
    EA_ATIME_NSEC,
    EA_BTIME_SEC,
    EA_BTIME_NSEC,
    EA_CTIME_NSEC,
    EA_CTIME_SEC,
    EA_MTIME_SEC,
    EA_MTIME_NSEC,
    EA_ATTRIBUTES,
    EA_RDEV_MAJOR,
    EA_RDEV_MINOR,
    EA_DEV_MAJOR,
    EA_DEV_MINOR,
    EA_MNT_ID,
};

struct statx_attr {
    /** Name of the column in the DB */
    const char *name;
    /** Excluded pattern used in SQL "upsert" */
    const char *excluded;
    /** Wrapper around sqlite3_bind_xxx to insert the actual value in the query
     */
    bool (*prepare_statement)(struct sqlite_cursor *, const void *);
};

static bool
bind_uint32(struct sqlite_cursor *cursor, const void *data)
{
    uint32_t value = *(uint32_t *)data;
    return sqlite_cursor_bind_int64(cursor, value);
}

static bool
bind_uint64(struct sqlite_cursor *cursor, const void *data)
{
    uint64_t value = *(uint64_t *)data;
    return sqlite_cursor_bind_int64(cursor, value);
}

static bool
bind_int64(struct sqlite_cursor *cursor, const void *data)
{
    int64_t value = *(int64_t *)data;
    return sqlite_cursor_bind_int64(cursor, value);
}

static bool
bind_mode(struct sqlite_cursor *cursor, const void *data)
{
    uint16_t value = *(uint16_t *)data;
    uint16_t mode = value & ~S_IFMT;

    return sqlite_cursor_bind_int64(cursor, mode);
}

static bool
bind_type(struct sqlite_cursor *cursor, const void *data)
{
    uint16_t value = *(uint16_t *)data;
    uint16_t type = value & S_IFMT;

    return sqlite_cursor_bind_int64(cursor, type);
}

static const struct statx_attr entries_attributes[] = {
    [EA_BLKSIZE]    = { "blksize",    "blksize=excluded.blksize",       bind_uint32 },
    [EA_NLINK]      = { "nlink",      "nlink=excluded.nlink",           bind_uint32 },
    [EA_UID]        = { "uid",        "uid=excluded.uid",               bind_uint32 },
    [EA_GID]        = { "gid",        "gid=excluded.gid",               bind_uint32 },
    [EA_MODE]       = { "mode",       "mode=excluded.mode",             bind_mode   },
    [EA_TYPE]       = { "type",       "type=excluded.type",             bind_type   },
    [EA_INO]        = { "ino",        "ino=excluded.ino",               bind_uint64 },
    [EA_SIZE]       = { "size",       "size=excluded.size",             bind_uint64 },
    [EA_BLOCKS]     = { "blocks",     "blocks=excluded.blocks",         bind_uint64 },
    [EA_ATTRIBUTES] = { "attributes", "attributes=excluded.attributes", bind_uint64 },
    [EA_ATIME_SEC]  = { "atime_sec",  "atime_sec=excluded.atime_sec",   bind_int64  },
    [EA_ATIME_NSEC] = { "atime_nsec", "atime_nsec=excluded.atime_nsec", bind_uint32 },
    [EA_BTIME_SEC]  = { "btime_sec",  "btime_sec=excluded.btime_sec",   bind_int64  },
    [EA_BTIME_NSEC] = { "btime_nsec", "btime_nsec=excluded.btime_nsec", bind_uint32 },
    [EA_CTIME_SEC]  = { "ctime_sec",  "ctime_sec=excluded.ctime_sec",   bind_int64  },
    [EA_CTIME_NSEC] = { "ctime_nsec", "ctime_nsec=excluded.ctime_nsec", bind_uint32 },
    [EA_MTIME_SEC]  = { "mtime_sec",  "mtime_sec=excluded.mtime_sec",   bind_int64  },
    [EA_MTIME_NSEC] = { "mtime_nsec", "mtime_nsec=excluded.mtime_nsec", bind_uint32 },
    [EA_RDEV_MAJOR] = { "rdev_major", "rdev_major=excluded.rdev_major", bind_uint32 },
    [EA_RDEV_MINOR] = { "rdev_minor", "rdev_minor=excluded.rdev_minor", bind_uint32 },
    [EA_DEV_MAJOR]  = { "dev_major",  "dev_major=excluded.dev_major",   bind_uint32 },
    [EA_DEV_MINOR]  = { "dev_minor",  "dev_minor=excluded.dev_minor",   bind_uint32 },
    [EA_MNT_ID]     = { "mnt_id",     "mnt_id=excluded.mnt_id",         bind_uint64 },
};

static const size_t base_size =
    sizeof("insert into entries (id, mask, ) values (?, ?, ) on conflict(id) do update set mask=excluded.mask;");

static enum entries_attribute
statx_field2ea(uint32_t attr)
{
    switch (attr) {
    case RBH_STATX_TYPE:
        return EA_TYPE;
    case RBH_STATX_MODE:
        return EA_MODE;
    case RBH_STATX_NLINK:
        return EA_NLINK;
    case RBH_STATX_UID:
        return EA_UID;
    case RBH_STATX_GID:
        return EA_GID;
    case RBH_STATX_ATIME_SEC:
        return EA_ATIME_SEC;
    case RBH_STATX_MTIME_SEC:
        return EA_MTIME_SEC;
    case RBH_STATX_CTIME_SEC:
        return EA_CTIME_SEC;
    case RBH_STATX_INO:
        return EA_INO;
    case RBH_STATX_SIZE:
        return EA_SIZE;
    case RBH_STATX_BLOCKS:
        return EA_BLOCKS;
    case RBH_STATX_BTIME_SEC:
        return EA_BTIME_SEC;
    case RBH_STATX_MNT_ID:
        return EA_MNT_ID;
    case RBH_STATX_BLKSIZE:
        return EA_BLKSIZE;
    case RBH_STATX_ATTRIBUTES:
        return EA_ATTRIBUTES;
    case RBH_STATX_ATIME_NSEC:
        return EA_ATIME_NSEC;
    case RBH_STATX_BTIME_NSEC:
        return EA_BTIME_NSEC;
    case RBH_STATX_CTIME_NSEC:
        return EA_CTIME_NSEC;
    case RBH_STATX_MTIME_NSEC:
        return EA_MTIME_NSEC;
    case RBH_STATX_RDEV_MAJOR:
        return EA_RDEV_MAJOR;
    case RBH_STATX_RDEV_MINOR:
        return EA_RDEV_MINOR;
    case RBH_STATX_DEV_MAJOR:
        return EA_DEV_MAJOR;
    case RBH_STATX_DEV_MINOR:
        return EA_DEV_MINOR;
    }
    __builtin_unreachable();
};

static inline const struct statx_attr *
bit2ea(int bit)
{
    enum entries_attribute ea;
    uint32_t field;

    field = bit2mask(bit);
    ea = statx_field2ea(field);

    return &entries_attributes[ea];
}

static const void *
statx_field(const struct rbh_fsevent *fsevent, uint32_t attr)
{
    const struct rbh_statx *statx = fsevent->upsert.statx;

    switch (attr) {
    case RBH_STATX_TYPE:
        return &statx->stx_mode;
    case RBH_STATX_MODE:
        return &statx->stx_mode;
    case RBH_STATX_NLINK:
        return &statx->stx_nlink;
    case RBH_STATX_UID:
        return &statx->stx_uid;
    case RBH_STATX_GID:
        return &statx->stx_gid;
    case RBH_STATX_ATIME_SEC:
        return &statx->stx_atime.tv_sec;
    case RBH_STATX_MTIME_SEC:
        return &statx->stx_mtime.tv_sec;
    case RBH_STATX_CTIME_SEC:
        return &statx->stx_ctime.tv_sec;
    case RBH_STATX_INO:
        return &statx->stx_ino;
    case RBH_STATX_SIZE:
        return &statx->stx_size;
    case RBH_STATX_BLOCKS:
        return &statx->stx_blocks;
    case RBH_STATX_BTIME_SEC:
        return &statx->stx_btime.tv_sec;
    case RBH_STATX_MNT_ID:
        return &statx->stx_mnt_id;
    case RBH_STATX_BLKSIZE:
        return &statx->stx_blksize;
    case RBH_STATX_ATTRIBUTES:
        return &statx->stx_attributes;
    case RBH_STATX_ATIME_NSEC:
        return &statx->stx_atime.tv_nsec;
    case RBH_STATX_BTIME_NSEC:
        return &statx->stx_btime.tv_nsec;
    case RBH_STATX_CTIME_NSEC:
        return &statx->stx_ctime.tv_nsec;
    case RBH_STATX_MTIME_NSEC:
        return &statx->stx_mtime.tv_nsec;
    case RBH_STATX_RDEV_MAJOR:
        return &statx->stx_rdev_major;
    case RBH_STATX_RDEV_MINOR:
        return &statx->stx_rdev_minor;
    case RBH_STATX_DEV_MAJOR:
        return &statx->stx_dev_major;
    case RBH_STATX_DEV_MINOR:
        return &statx->stx_dev_minor;
    }
    __builtin_unreachable();
}

static size_t
upsert_query_size(const struct rbh_fsevent *fsevent, bool has_xattrs)
{
    uint32_t mask = fsevent->upsert.statx->stx_mask;
    const char *symlink = fsevent->upsert.symlink;
    size_t query_size = base_size;
    size_t nb_attrs = 0;
    int i;

    foreach_bit_set(mask, i) {
        const struct statx_attr *attr = bit2ea(i);

        query_size += strlen(attr->name);
        query_size += strlen(attr->excluded);
        nb_attrs++;
    }
    query_size += 4 * (nb_attrs - 1); // coma between attribute names and '?'
    query_size += nb_attrs; // 1 '?' per attribute
    query_size += 2 * (nb_attrs - 1); // extra space between the excluded
    if (has_xattrs) {
        query_size += sizeof(", xattrs");
        query_size += sizeof(", ?");
        query_size +=
            sizeof(", xattrs=json_patch(entries.xattrs, excluded.xattrs)");
    }

    if (symlink) {
        query_size += sizeof(", symlink");
        query_size += sizeof(", ?");
        query_size += sizeof("symlink=excluded.symlink");
    }

    return query_size;
}

static bool
fsevent_has_xattrs(const struct rbh_fsevent *fsevent)
{
    for (size_t i = 0; i < fsevent->xattrs.count; i++) {
        const struct rbh_value_pair *xattr;

        xattr = &fsevent->xattrs.pairs[i];
        if (!strncmp(xattr->key, "ns.", 3))
            continue;

        return true;
    }

    return false;
}

static const char *
build_upsert_query(const struct rbh_fsevent *fsevent, bool has_xattrs)
{
    uint32_t mask = fsevent->upsert.statx->stx_mask;
    size_t size = upsert_query_size(fsevent, has_xattrs);
    const char *symlink = fsevent->upsert.symlink;
    char *query;
    int i;

    query = malloc(size + 1);
    if (!query)
        return NULL;

    strncpy(query, "insert into entries (id, mask, ", size);
    foreach_bit_set(mask, i) {
        const struct statx_attr *attr = bit2ea(i);
        uint32_t field = bit2mask(i);

        strncat(query, attr->name, size);

        if (mask & ~field)
            /* do not insert ',' for the last parameter */
            strncat(query, ", ", size);
    }
    if (has_xattrs)
        strncat(query, ", xattrs", size);
    if (symlink)
        strncat(query, ", symlink", size);


    strncat(query, ") values (?, ?, ", size);
    if (has_xattrs)
        strncat(query, "?, ", size);
    if (symlink)
        strncat(query, "?, ", size);

    mask = fsevent->upsert.statx->stx_mask;
    foreach_bit_set(mask, i) {
        uint32_t field;

        field = bit2mask(i);

        strncat(query, "?", size);
        if (mask & ~field)
            /* do not insert ',' for the last parameter */
            strncat(query, ", ", size);
    }
    strncat(query, ") on conflict(id) do update set mask=excluded.mask, ", size);
    mask = fsevent->upsert.statx->stx_mask;
    foreach_bit_set(mask, i) {
        const struct statx_attr *attr = bit2ea(i);
        uint32_t field = bit2mask(i);

        strncat(query, attr->excluded, size);

        if (mask & ~field)
            /* do not insert ' ' for the last parameter */
            strncat(query, ", ", size);
    }
    if (has_xattrs)
        strncat(query, ", xattrs=json_patch(entries.xattrs, excluded.xattrs)",
                size);
    if (symlink)
        strncat(query, ", symlink=excluded.symlink", size);

    return query;
}

static bool
sqlite_process_upsert(struct sqlite_backend *sqlite,
                      const struct rbh_fsevent *fsevent)
{
    bool has_xattrs = fsevent_has_xattrs(fsevent);
    const char *insert = build_upsert_query(fsevent, has_xattrs);
    uint32_t mask = fsevent->upsert.statx->stx_mask;
    struct sqlite_cursor *cursor = &sqlite->cursor;
    const char *symlink = fsevent->upsert.symlink;
    const struct rbh_id *id = &fsevent->id;
    int save_errno;
    bool res;
    int i;

    if (!insert)
        return false;

    if (!(sqlite_setup_query(cursor, insert) &&
          sqlite_cursor_bind_id(cursor, id) &&
          sqlite_cursor_bind_int64(cursor, fsevent->upsert.statx->stx_mask)))
        goto free_insert;

    foreach_bit_set(mask, i) {
        const struct statx_attr *attr = bit2ea(i);
        uint32_t field = bit2mask(i);

        res = attr->prepare_statement(cursor, statx_field(fsevent, field));
        if (!res)
            goto free_insert;
    }
    if (has_xattrs) {
        const char *xattrs = sqlite_xattr2json(&fsevent->xattrs);

        res = sqlite_cursor_bind_string(cursor, xattrs);
        free((void *)xattrs);
        if (!res)
            goto free_insert;
    }
    if (symlink) {
        if (!sqlite_cursor_bind_string(cursor, symlink))
            goto free_insert;
    }

    if (!sqlite_cursor_exec(cursor))
        goto free_insert;

    free((void *)insert);
    return true;

free_insert:
    save_errno = errno;
    free((void *)insert);
    errno = save_errno;

    return false;
}

static const char *
link_fsevent_xattr_path(const struct rbh_fsevent *link)
{
    const char *path = rbh_fsevent_path(link);
    json_t *path_object;

    assert(path); /* links must have a path at this point */

    path_object = json_pack("{ss}", "path", path);

    return json_dumps(path_object, JSON_COMPACT);
}

static bool
sqlite_process_unlink(struct sqlite_backend *sqlite,
                      const struct rbh_fsevent *fsevent)
{
    const char *query =
        fsevent->link.parent_id->size > 0 ?
            "delete from ns where id = ? and parent_id = ? and name = ?" :
            "delete from ns where id = ? and parent_id is NULL and name = ?";
    struct sqlite_cursor *cursor = &sqlite->cursor;

    return sqlite_setup_query(cursor, query) &&
        sqlite_cursor_bind_id(cursor, &fsevent->id) &&
        (fsevent->link.parent_id->size > 0 ?
            sqlite_cursor_bind_id(cursor, fsevent->link.parent_id) :
            true) &&
        sqlite_cursor_bind_string(cursor, fsevent->link.name) &&
        sqlite_cursor_exec(cursor);
}

static bool
sqlite_process_link(struct sqlite_backend *sqlite,
                    const struct rbh_fsevent *fsevent)
{
    const char *query =
        "insert into ns (id, parent_id, name, xattrs) "
        "values (?, ?, ?, ?) on conflict(id, parent_id, name) do "
        "update set xattrs = excluded.xattrs";
    const char *path = link_fsevent_xattr_path(fsevent);
    struct sqlite_cursor *cursor = &sqlite->cursor;
    bool res;

    assert(path);

    /* The root has a NULL parent_id which disables the check for unique
     * primary key. Therefore, we need to delete it explicitly before the
     * link to avoid duplication. NULL in a column of a primary key should not
     * be allowed in SQL but SQLite supports it.
     */
    if (path[0] == '/' && path[1] == '\0' &&
        !sqlite_process_unlink(sqlite, fsevent)) {
        int save_errno = errno;

        free((void *)path);
        errno = save_errno;
        return false;
    }

    res = sqlite_setup_query(cursor, query) &&
        sqlite_cursor_bind_id(cursor, &fsevent->id) &&
        sqlite_cursor_bind_id(cursor, fsevent->link.parent_id) &&
        sqlite_cursor_bind_string(cursor, fsevent->link.name) &&
        sqlite_cursor_bind_string(cursor, path) &&
        sqlite_cursor_exec(cursor);
    free((void *)path);

    return res;
}

static bool
sqlite_process_ns_xattr(struct sqlite_backend *sqlite,
                        const struct rbh_fsevent *fsevent)
{
    const char *query =
        "insert into ns (id, parent_id, name, xattrs) "
        "values (?, ?, ?, ?) on conflict(id, parent_id, name) do "
        "update set xattrs = json_patch(ns.xattrs, excluded.xattrs)";
    struct sqlite_cursor *cursor = &sqlite->cursor;
    const char *xattrs;
    bool res;

    xattrs = sqlite_xattr2json(&fsevent->xattrs);
    if (!xattrs)
        return false;

    res = sqlite_setup_query(cursor, query) &&
        sqlite_cursor_bind_id(cursor, &fsevent->id) &&
        sqlite_cursor_bind_id(cursor, fsevent->ns.parent_id) &&
        sqlite_cursor_bind_string(cursor, fsevent->ns.name) &&
        sqlite_cursor_bind_string(cursor, xattrs) &&
        sqlite_cursor_exec(cursor);
    free((void *)xattrs);

    return res;
}

static bool
sqlite_process_xattr(struct sqlite_backend *sqlite,
                     const struct rbh_fsevent *fsevent)
{
    const char *query =
        "insert into entries (id, xattrs) "
        "values (?, ?) on conflict(id) do "
        "update set xattrs=json_patch(ns.xattrs, excluded.xattrs)";
    struct sqlite_cursor *cursor = &sqlite->cursor;
    const char *xattrs;
    bool res;

    xattrs = sqlite_xattr2json(&fsevent->xattrs);
    if (!xattrs)
        return false;

    res = sqlite_setup_query(cursor, query) &&
        sqlite_cursor_bind_id(cursor, &fsevent->id) &&
        sqlite_cursor_bind_string(cursor, xattrs) &&
        sqlite_cursor_exec(cursor);
    free((void *)xattrs);

    return res;
}

static bool
sqlite_process_delete(struct sqlite_backend *sqlite,
                      const struct rbh_fsevent *fsevent)
{
    struct sqlite_cursor *cursor = &sqlite->cursor;

    return sqlite_setup_query(cursor, "delete from entries where id = ?") &&
        sqlite_cursor_bind_id(cursor, &fsevent->id) &&
        sqlite_cursor_exec(cursor) &&

        sqlite_setup_query(cursor, "delete from ns where id = ?") &&
        sqlite_cursor_bind_id(cursor, &fsevent->id) &&
        sqlite_cursor_exec(cursor);
}

static bool
sqlite_process_fsevent(struct sqlite_backend *sqlite,
                       const struct rbh_fsevent *fsevent)
{
    switch (fsevent->type) {
    case RBH_FET_LINK:
        return sqlite_process_link(sqlite, fsevent);
    case RBH_FET_UNLINK:
        return sqlite_process_unlink(sqlite, fsevent);
    case RBH_FET_UPSERT:
        return sqlite_process_upsert(sqlite, fsevent);
    case RBH_FET_XATTR:
        if (fsevent->ns.parent_id)
            return sqlite_process_ns_xattr(sqlite, fsevent);

        return sqlite_process_xattr(sqlite, fsevent);
    case RBH_FET_DELETE:
        return sqlite_process_delete(sqlite, fsevent);
    }
    return true;
}

ssize_t
sqlite_backend_update(void *backend, struct rbh_iterator *fsevents)
{
    struct sqlite_backend *sqlite = backend;
    ssize_t count = 0;

    if (!fsevents)
        return 0;

    sqlite_cursor_setup(sqlite, &sqlite->cursor);

    do {
        const struct rbh_fsevent *fsevent;

        errno = 0;
        fsevent = rbh_iter_next(fsevents);
        if (fsevent == NULL) {
            if (errno == ENODATA)
                break;

            return -1;
        }

        if (!sqlite_process_fsevent(sqlite, fsevent))
            return -1;

        count++;
    } while (true);

    return count;
}
