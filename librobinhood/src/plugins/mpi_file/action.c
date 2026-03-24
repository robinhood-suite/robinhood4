/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood/plugins/backend.h>
#include <robinhood/action.h>
#include <robinhood/fsentry.h>

#include "plugin_callback_common.h"
#include "mpi_file.h"

int
rbh_mpi_file_delete_entry(struct rbh_backend *backend,
                          struct rbh_fsentry *fsentry,
                          const struct rbh_delete_params *params)
{
    if (posix_plugin == NULL)
        if (import_posix_plugin())
            return -1;

    return rbh_pe_common_ops_delete_entry(posix_plugin->common_ops,
                                          backend, fsentry, params);
}
