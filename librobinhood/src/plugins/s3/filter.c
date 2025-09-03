/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdbool.h>
#include <stddef.h>

#include "robinhood/plugins/backend.h"
#include "plugin_callback_common.h"

struct rbh_filter *
rbh_s3_build_filter(const char **argv, int argc, int *index,
                    bool *need_prefetch)
{
    if (posix_plugin == NULL)
        if (import_posix_plugin())
            return NULL;

    return rbh_pe_common_ops_build_filter(posix_plugin->common_ops, argv, argc,
                                          index, need_prefetch);
}
