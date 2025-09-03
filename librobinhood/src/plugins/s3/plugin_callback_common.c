/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <stdlib.h>

#include <robinhood/plugins/backend.h>

#include "plugin_callback_common.h"

const struct rbh_backend_plugin *posix_plugin;

int
import_posix_plugin()
{
    posix_plugin = rbh_backend_plugin_import("posix");
    if (posix_plugin == NULL)
        return 1;

    return 0;
}
