/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

struct rbh_backend *
rbh_ldiskfs_backend_new(const struct rbh_backend_plugin *self,
                        const struct rbh_uri *uri,
                        struct rbh_config *config,
                        bool read_only)
{
    return NULL;
}

void
ldiskfs_backend_destroy(void *backend)
{
}
