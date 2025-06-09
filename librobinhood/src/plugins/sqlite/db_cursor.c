/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

/**
 * SQLite cursor: setup/run/step SQLite queries
 *
 * struct sqlite_cursor cursor;
 * struct sqlite_backend *sqlite = backend;
 *
 * // initialize cursor
 * sqlite_cursor_setup(sqlite, &cursor);
 *
 * // setup query
 * sqlite_setup_query(&cursor, "select * from entries");
 *
 * // step to fetch next row
 * // -> return true and errno = 0 when there are no more rows to read
 * // -> return true and errno = EAGAIN otherwise
 * sqlite_cursor_step(&cursor);
 *
 * // only necessary if step is used
 * sqlite_cursor_fini(&cursor);
 *
 * // sqlite_cursor_setup not needed here, a cursor is setup only once
 * // and can be reused to run additional queries
 * sqlite_setup_query(&cursor,
 *                    "delete from ns where id = ? parent_id = ? and name = ?");
 *
 * // bind values to the query
 * sqlite_cursor_bind_id(&cursor, &id);
 * sqlite_cursor_bind_id(&cursor, &parent_id);
 * sqlite_cursor_bind_string(&cursor, name);
 *
 * // run the query: since there is no data returned by the query, use exec()
 * // instead of step()
 * sqlite_cursor_exec(&cursor);
 *
 * // no need to call fini(), exec() does the cleanup for us
 */

bool
sqlite_cursor_setup(struct sqlite_backend *backend,
                    struct sqlite_cursor *cursor)
{
    cursor->db = backend->db;
    return true;
}

bool
sqlite_setup_query(struct sqlite_cursor *cursor, const char *query)
{
    int rc;

    rc = sqlite3_prepare_v2(cursor->db, query, -1, &cursor->stmt, NULL);
    if (rc != SQLITE_OK)
        /* Limit the size used by the query to avoid truncating the error at
         * the end. This leaves 256 bytes for the sqlite error and this string.
         */
        return sqlite_db_fail(cursor->db, "failed to prepare query '%.*s'",
                              256, query);

    /* SQLite3 indices start at 1 */
    cursor->index = 1;
    /* column starts at 0 */
    cursor->col = 0;

    return true;
}

bool
sqlite_cursor_fini(struct sqlite_cursor *cursor)
{
    sqlite3_finalize(cursor->stmt);
    return true;
}

bool
sqlite_cursor_exec(struct sqlite_cursor *cursor)
{
    int rc;

    rc = sqlite3_step(cursor->stmt);
    if (rc != SQLITE_OK && rc != SQLITE_DONE)
        return sqlite_db_fail(cursor->db, "failed to run sqlite statement");

    sqlite3_finalize(cursor->stmt);

    return true;
}

bool
sqlite_cursor_step(struct sqlite_cursor *cursor)
{
    int rc;

    rc = sqlite3_step(cursor->stmt);
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE)
        return sqlite_db_fail(cursor->db, "failed to step sqlite statement");

    errno = 0;
    /* reset column to read current row */
    cursor->col = 0;
    if (rc == SQLITE_OK || rc == SQLITE_DONE)
        return true;

    errno = EAGAIN;
    return true;
}
