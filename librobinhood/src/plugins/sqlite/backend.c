/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static const struct rbh_backend_operations SQLITE_BACKEND_OPS = {
    .update  = sqlite_backend_update,
    .destroy = sqlite_backend_destroy,
};

static const struct rbh_backend SQLITE_BACKEND = {
    .id   = RBH_BI_SQLITE,
    .name = RBH_SQLITE_BACKEND_NAME,
    .ops  = &SQLITE_BACKEND_OPS,
};

struct rbh_backend *
rbh_sqlite_backend_new(const struct rbh_backend_plugin *self,
                       const struct rbh_uri *uri,
                       struct rbh_config *config,
                       bool read_only)
{
    struct sqlite_backend *sqlite;

    sqlite = calloc(1, sizeof(*sqlite));
    if (!sqlite)
        return NULL;

    sqlite->backend = SQLITE_BACKEND;
    if (!sqlite_backend_open(sqlite, uri->fsname, read_only))
        return NULL;

    return &sqlite->backend;
}

void
sqlite_backend_destroy(void *backend)
{
    struct sqlite_backend *sqlite = backend;

    sqlite_backend_close(sqlite);
    free(sqlite);
}
