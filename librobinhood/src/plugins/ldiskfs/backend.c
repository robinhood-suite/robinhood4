/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static const struct rbh_backend_operations LDISKFS_BACKEND_OPS = {
};

static const struct rbh_backend LDISKFS_BACKEND = {
    .id   = RBH_BI_LDISKFS,
    .name = RBH_LDISKFS_BACKEND_NAME,
    .ops  = &LDISKFS_BACKEND_OPS,
};

struct rbh_backend *
rbh_ldiskfs_backend_new(const struct rbh_backend_plugin *self,
                        const struct rbh_uri *uri,
                        struct rbh_config *config,
                        bool read_only)
{
    struct ldiskfs_backend *ldiskfs;

    ldiskfs = calloc(1, sizeof(*ldiskfs));
    if (!ldiskfs)
        return NULL;

    ldiskfs->backend = LDISKFS_BACKEND;

    return &ldiskfs->backend;
}

void
ldiskfs_backend_destroy(void *backend)
{
    struct ldiskfs_backend *ldiskfs = backend;

    free(ldiskfs);
}
