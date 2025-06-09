/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static const char *RBH_SQLITE_SCHEMA_CODE = "";

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
