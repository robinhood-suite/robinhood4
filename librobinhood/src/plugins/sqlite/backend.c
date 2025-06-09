/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

const struct rbh_backend_operations SQLITE_BACKEND_OPS = {
    .branch        = sqlite_backend_branch,
    .root          = sqlite_backend_root,
    .destroy       = sqlite_backend_destroy,
    .filter        = sqlite_backend_filter,
    .insert_source = sqlite_backend_insert_source,
    .get_info      = sqlite_backend_get_info,
    .update        = sqlite_backend_update,
};

static const struct rbh_backend SQLITE_BACKEND = {
    .id   = RBH_BI_SQLITE,
    .name = RBH_SQLITE_BACKEND_NAME,
    .ops  = &SQLITE_BACKEND_OPS,
};

struct rbh_backend *
rbh_sqlite_backend_new(const struct rbh_backend_plugin *self,
                       const char *type,
                       const char *path,
                       struct rbh_config *config,
                       bool read_only)
{
    struct sqlite_backend *sqlite;

    sqlite = calloc(1, sizeof(*sqlite));
    if (!sqlite)
        return NULL;

    sqlite->backend = SQLITE_BACKEND;
    if (!sqlite_backend_open(sqlite, path, read_only))
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
