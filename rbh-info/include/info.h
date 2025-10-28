/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_INFO_H
#define RBH_INFO_H

#include <robinhood/plugin.h>
#include <robinhood/plugins/backend.h>

/**
 * List the plugins and extensions currently installed.
 */
void
list_plugins_and_extensions();

/**
 * Show the capabilities of a given plugin.
 *
 * @param plugin   The plugin whose capabilities should be shown
 *
 * @return         0 on success, 1 on error
 */
int
capabilities_translate(const char *plugin);

/**
 * Show the information requested in \p flags using the backend \p from.
 *
 * @param from     Where to fetch the information
 * @param flags    The information to fetch and show
 *
 * @return         0 on success, 1 on error
 */
int
print_info_fields(struct rbh_backend *from, int flags);

void
info_translate(const struct rbh_backend_plugin *plugin);

#endif
