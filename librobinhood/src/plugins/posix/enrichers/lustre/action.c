/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <inttypes.h>
#include <stdio.h>

#include <lustre/lustreapi.h>
#include <robinhood/action.h>
#include <robinhood/backend.h>
#include <robinhood/utils.h>
#include <robinhood.h>

static int
rbh_lustre_log_entry(struct rbh_fsentry *entry,
                     const struct rbh_value_map *params)
{
    const struct lu_fid *fid = NULL;
    const char *path = "(NULL)";
    char *params_str = NULL;

    /* Path */
    if (entry) {
        const char *rel_path = fsentry_relative_path(entry);
        if (rel_path)
            path = rel_path;

        /* FID */
        fid = rbh_lu_fid_from_id(&entry->id);
    }

    /* Parameters */
    if (params && params->count > 0) {
        params_str = rbh_value_map_to_string(params);
    }

    if (fid)
        printf("LogAction | fid=" DFID ", path=%s, params=%s\n",
               PFID(fid), path, params_str ? params_str : "{}");
    else
        printf("LogAction | fid=(NULL), path=%s, params=%s\n",
               path, params_str ? params_str : "{}");

    free(params_str);
    return 0;
}

int
rbh_lustre_apply_action(const struct rbh_action *action,
                        struct rbh_fsentry *entry,
                        struct rbh_backend *mi_backend,
                        struct rbh_backend *fs_backend)
{
    switch (action->type) {
    case RBH_ACTION_LOG:
        return rbh_lustre_log_entry(entry, &action->params);
    default:
        errno = ENOTSUP;
        return -1;
    }
}

