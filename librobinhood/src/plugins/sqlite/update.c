/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static bool
sqlite_process_link(struct sqlite_backend *sqlite,
                    const struct rbh_fsevent *fsevent)
{
    return true;
}

static bool
sqlite_process_unlink(struct sqlite_backend *sqlite,
                      const struct rbh_fsevent *fsevent)
{
    return true;
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
