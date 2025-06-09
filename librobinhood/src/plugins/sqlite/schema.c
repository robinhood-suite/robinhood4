/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static const char *RBH_SQLITE_SCHEMA_CODE =
"create table version("
"    id      INTEGER," // fake ID (always one) to make sure we have only one
"    major   INTEGER," // row
"    minor   INTEGER,"
"    release INTEGER,"
"    primary key (id)"
");"
"create table entries("
"    id         BLOB primary key," // struct rbh_id
"    mask       INT,"
"    blksize    INT,"
"    nlink      INT,"
"    uid        INT,"
"    gid        INT,"
"    mode       INT,"
"    type       INT,"
"    ino        INT,"
"    size       INT,"
"    blocks     INT,"
"    attributes INT,"
"    atime_sec  INT,"
"    atime_nsec INT,"
"    btime_sec  INT,"
"    btime_nsec INT,"
"    ctime_sec  INT,"
"    ctime_nsec INT,"
"    mtime_sec  INT,"
"    mtime_nsec INT,"
"    rdev_major INT,"
"    rdev_minor INT,"
"    dev_major  INT,"
"    dev_minor  INT,"
"    mnt_id     INT,"
"    symlink    TEXT,"
"    xattrs     TEXT"  // json
");"
"create table ns("
"    id         BLOB," // struct rbh_id
"    parent_id  BLOB," // struct rbh_id
"    name       TEXT,"
"    xattrs     TEXT," // json
"    primary key (id, parent_id, name)"
");"
"create table info("
"    id         INT,"  // fake ID (always one) to make sure we have only one
"    plugin     TEXT," // row
"    extensions TEXT," // json array
"    primary key (id)"
");";

static bool
get_version(struct sqlite_backend *sqlite)
{
    const char *query =
        "select major, minor, release from version where id = 1";
    struct sqlite_cursor cursor;
    int64_t release;
    int64_t major;
    int64_t minor;

    if (!(sqlite_cursor_setup(sqlite, &cursor) &&
          sqlite_setup_query(&cursor, query) &&
          sqlite_cursor_step(&cursor)))
        return false;

    major = sqlite_cursor_get_int64(&cursor);
    minor = sqlite_cursor_get_int64(&cursor);
    release = sqlite_cursor_get_int64(&cursor);
    sqlite_cursor_fini(&cursor);

    // We cannot use the RPV macro here as it uses UINT64_C which only
    // work on integer literals.
    sqlite->version =
        ((major << RPV_MAJOR_SHIFT) + (minor << RPV_MINOR_SHIFT) + release);

    return true;
}

static bool
set_version(struct sqlite_backend *sqlite)
{
    const char *query =
        "insert or replace into version (id, major, minor, release)"
        " values (1, ?, ?, ?)";
    struct sqlite_cursor cursor;

    sqlite->version = RBH_SQLITE_BACKEND_VERSION;

    return sqlite_cursor_setup(sqlite, &cursor) &&
        sqlite_setup_query(&cursor, query) &&
        sqlite_cursor_bind_int64(&cursor, RBH_SQLITE_BACKEND_MAJOR) &&
        sqlite_cursor_bind_int64(&cursor, RBH_SQLITE_BACKEND_MINOR) &&
        sqlite_cursor_bind_int64(&cursor, RBH_SQLITE_BACKEND_RELEASE) &&
        sqlite_cursor_exec(&cursor);
}

static bool
setup_schema(struct sqlite_backend *sqlite)
{
    int rc;

    rc = sqlite3_exec(sqlite->db, RBH_SQLITE_SCHEMA_CODE, NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        return sqlite_db_fail(sqlite->db,
                              "Failed to create schema of '%s'",
                              sqlite->path);

    return set_version(sqlite);
}

bool
sqlite_backend_dup(struct sqlite_backend *sqlite,
                   struct sqlite_backend *dup)
{
    dup->sstack = rbh_sstack_new(PAGE_SIZE);
    if (!dup->sstack)
        return NULL;

    dup->path = rbh_sstack_strdup(dup->sstack, sqlite->path);
    if (!dup->path) {
        rbh_sstack_destroy(dup->sstack);
        return NULL;
    }
    return sqlite_backend_open(dup, sqlite->path, sqlite->read_only);
}

static bool
load_modules(sqlite3 *db)
{
    const char *modules[] = {
        "/usr/lib64/sqlite3/pcre.so",
    };
    int rc;

    rc = sqlite3_enable_load_extension(db, 1);
    if (rc != SQLITE_OK)
        return sqlite_db_fail(db, "failed to enable module loading");

    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
        char *err_msg;

        rc = sqlite3_load_extension(db, modules[i], 0, &err_msg);
        if (rc != SQLITE_OK)
            return sqlite_db_fail(db, "failed to load '%s': %s",
                                  modules[i], err_msg);
    }

    return true;
}

bool
sqlite_backend_open(struct sqlite_backend *sqlite,
                    const char *path,
                    bool read_only)
{
    int mode = read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE;
    int rc;

    sqlite->read_only = read_only;
    rc = sqlite3_open_v2(path, &sqlite->db, mode, NULL);
    if (rc == SQLITE_OK)
        goto ok;

    if (rc != SQLITE_CANTOPEN) /* CANTOPEN: file does not exist */
        return sqlite_fail("Failed to open '%s'", path);

    rc = sqlite3_open_v2(path, &sqlite->db, mode | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK)
        return sqlite_fail("Failed to create db at '%s'", path);

    if (!setup_schema(sqlite)) {
        sqlite3_close_v2(sqlite->db);
        return false;
    }

ok:
    if (!sqlite->version) {
        if (!get_version(sqlite))
            return sqlite_db_fail(sqlite->db,
                    "failed to retrieve version from db '%s'",
                    path);
    }
    if (!(load_modules(sqlite->db) &&
          setup_custom_functions(sqlite->db))) {
        sqlite3_close_v2(sqlite->db);
        return false;
    }

    /* sqlite will set errno to ENOENT if the DB does not exist but it will
     * create it so reset errno.
     */
    errno = 0;
    return true;
}

void
sqlite_backend_close(struct sqlite_backend *sqlite)
{
    sqlite3_close_v2(sqlite->db);
}
