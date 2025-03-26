/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static const char *
link_fsevent_xattr_path(const struct rbh_fsevent *link)
{
    const char *path = rbh_fsevent_path(link);
    json_t *path_object;
    char *res;

    assert(path); /* links must have a path at this point */

    path_object = json_pack("{ss}", "path", path);

    res = json_dumps(path_object, JSON_COMPACT);
    json_decref(path_object);
    return res;
}

static bool
sqlite_process_unlink(struct sqlite_backend *sqlite,
                      const struct rbh_fsevent *fsevent)
{
    return true;
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
sqlite_process_upsert(struct sqlite_backend *sqlite,
                      const struct rbh_fsevent *fsevent)
{
    return true;
}

static bool
sqlite_process_ns_xattr(struct sqlite_backend *sqlite,
                        const struct rbh_fsevent *fsevent)
{
    return true;
}

static bool
sqlite_process_xattr(struct sqlite_backend *sqlite,
                     const struct rbh_fsevent *fsevent)
{
    return true;
}

static bool
sqlite_process_delete(struct sqlite_backend *sqlite,
                      const struct rbh_fsevent *fsevent)
{
    return true;
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
