/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/backends/sqlite.h"

struct rbh_backend *
rbh_sqlite_backend_new(const struct rbh_backend_plugin *self,
                       const char *type,
                       const char *path,
                       struct rbh_config *config,
                       bool read_only)
{
    return NULL;
}

void
sqlite_backend_destroy(void *backend)
{
}
