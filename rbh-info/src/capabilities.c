/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>

#include "info.h"

int
capabilities_translate(const char *_plugin)
{
    const struct rbh_backend_plugin *plugin =
        rbh_backend_plugin_import(_plugin);
    uint8_t capabilities;

    if (plugin == NULL) {
        fprintf(stderr, "Failed to import plugin '%s': %s (%d)\n",
                _plugin, strerror(errno), errno);
        return EINVAL;
    }

    capabilities = plugin->capabilities;
    printf("Capabilities of %s:\n", plugin->plugin.name);

    if (capabilities & RBH_FILTER_OPS)
        printf("- filter: rbh-find [source]\n");
    if (capabilities & RBH_SYNC_OPS)
        printf("- synchronisation: rbh-sync [source]\n");
    if (capabilities & RBH_UPDATE_OPS)
        printf("- update: rbh-sync [target]\n");
    if (capabilities & RBH_BRANCH_OPS)
        printf("- branch: rbh-sync [source for partial processing]\n");

    return 0;
}
