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
#include <sysexits.h>

#include "robinhood/plugins/backend.h"
#include "plugin_callback_common.h"

static struct rbh_filter *
bucket2filter(const char *bucket)
{
    struct rbh_filter_field field = {
        .fsentry = RBH_FP_INODE_XATTRS,
        .xattr = "bucket"
    };

    return rbh_shell_regex2filter(&field, bucket, RBH_RO_SHELL_PATTERN);
}

struct rbh_filter *
rbh_s3_build_filter(const char **argv, int argc, int *index,
                    bool *need_prefetch)
{
    struct rbh_filter *filter;
    int i = *index;

    if (posix_plugin == NULL)
        if (import_posix_plugin())
            return NULL;

    if (strcmp(argv[i], "-bucket") == 0) {
        if (i + 1 >= argc)
            error(EX_USAGE, 0, "missing arguments to '%s'", argv[i]);
        filter = bucket2filter(argv[++i]);
        *index = i;
    } else {
        filter = rbh_pe_common_ops_build_filter(posix_plugin->common_ops, argv,
                                                argc, index, need_prefetch);
    }

    assert(filter != NULL);

    return filter;
}
