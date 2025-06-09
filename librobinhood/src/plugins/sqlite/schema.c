/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static const char *RBH_SQLITE_SCHEMA_CODE =
"create table version("
"    version INTEGER"
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
");";

static bool
setup_schema(struct sqlite_backend *sqlite)
{
    int rc;

    rc = sqlite3_exec(sqlite->db, RBH_SQLITE_SCHEMA_CODE, NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        return sqlite_db_fail(sqlite->db,
                              "Failed to create schema of '%s'",
                              sqlite->path);

    return true;
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

    sqlite->path = path;
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
