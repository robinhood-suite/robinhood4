/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood/plugins/backend.h>
#include <robinhood/fsentry.h>

#include "plugin_callback_common.h"

int
rbh_mpi_file_delete_entry(struct rbh_fsentry *fsentry)
{
    if (posix_plugin == NULL)
        if (import_posix_plugin())
            return -1;

    return rbh_plugin_delete_entry(posix_plugin, fsentry);
}
