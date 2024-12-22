/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef ROBINHOOD_POSIX_EXTENTION_H
#define ROBINHOOD_POSIX_EXTENTION_H

#include <robinhood/plugin.h>
#include <robinhood/plugins/backend.h>

struct rbh_value_pair;
struct rbh_statx;
struct entry_info;
struct rbh_sstack;

typedef int (*enricher_t)(struct entry_info *einfo,
                          uint64_t flags,
                          struct rbh_value_pair *pairs,
                          size_t pairs_count,
                          struct rbh_sstack *values);

typedef int (*iter_new_t)(void);

struct rbh_posix_extension {
    struct rbh_plugin_extension extension;
    iter_new_t iter_new;
    enricher_t enrich;
    int (*setup_enricher)(void);
};

static inline const struct rbh_posix_extension *
rbh_posix_load_extension(const struct rbh_plugin *plugin, const char *name)
{
    const struct rbh_posix_extension *extension;

    extension = (const struct rbh_posix_extension *)
        rbh_plugin_load_extension(plugin, name);

    return extension;
}

#endif
