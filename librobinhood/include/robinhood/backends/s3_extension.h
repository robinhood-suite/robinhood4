/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_S3_EXTENSION_H
#define ROBINHOOD_S3_EXTENSION_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/backends/common.h>
#include <robinhood/iterator.h>
#include <robinhood/plugins/backend.h>
#include <robinhood/plugin.h>

typedef struct rbh_mut_iterator *(*iter_new_t)(const char *, const char *, int);

struct s3_backend {
    struct rbh_backend backend;
    struct s3_iterator *(*iter_new)();
};

struct rbh_s3_extension {
    struct rbh_plugin_extension extension;
    struct s3_iterator *(*iter_new)();
};

static inline const struct rbh_s3_extension *
rbh_s3_load_extension(const struct rbh_plugin *plugin, const char *name)
{
    return (const struct rbh_s3_extension *)
        rbh_plugin_load_extension(plugin, name);
}

#endif
