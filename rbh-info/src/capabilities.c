/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <stdio.h>

#include "info.h"

void
capabilities_translate(const struct rbh_backend_plugin *plugin)
{
    const uint8_t capabilities = plugin->capabilities;

    printf("Capabilities of %s:\n", plugin->plugin.name);

    if (capabilities & RBH_FILTER_OPS)
        printf("- filter: rbh-find [source]\n");
    if (capabilities & RBH_SYNC_OPS)
        printf("- synchronisation: rbh-sync [source]\n");
    if (capabilities & RBH_UPDATE_OPS)
        printf("- update: rbh-sync [target]\n");
    if (capabilities & RBH_BRANCH_OPS)
        printf("- branch: rbh-sync [source for partial processing]\n");
}
