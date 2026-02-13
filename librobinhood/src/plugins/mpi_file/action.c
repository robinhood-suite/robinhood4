/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood/plugins/backend.h>
#include <robinhood/action.h>
#include <robinhood/fsentry.h>

#include "plugin_callback_common.h"

static int
rbh_mpi_file_delete_entry(const struct rbh_action *action,
                          struct rbh_fsentry *entry,
                          struct rbh_backend *mi_backend,
                          struct rbh_backend *fs_backend)
{
    if (posix_plugin == NULL)
        if (import_posix_plugin())
            return -1;

    return rbh_pe_common_ops_apply_action(posix_plugin->common_ops, action,
                                          entry, mi_backend, fs_backend);
}

int
rbh_mpi_file_apply_action(const struct rbh_action *action,
                          struct rbh_fsentry *entry,
                          struct rbh_backend *mi_backend,
                          struct rbh_backend *fs_backend)
{
    switch (action->type) {
    case RBH_ACTION_DELETE:
        return rbh_mpi_file_delete_entry(action, entry, mi_backend,
                                         fs_backend);
    default:
        errno = ENOTSUP;
        return -1;
    }
}
