/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <lustre/lustreapi.h>
#include <robinhood/action.h>
#include <robinhood/backend.h>
#include <robinhood.h>
#include "../../posix_internals.h"

static int
rbh_lustre_log_entry(struct rbh_fsentry *entry,
                     const struct rbh_value_map *params,
                     struct rbh_sstack *sstack)
{
    const struct lu_fid *fid = NULL;
    const char *path = "(NULL)";
    json_t *json_params = NULL;
    char *json_str = NULL;

    /* Path */
    if (entry) {
        const char *rel_path = fsentry_relative_path(entry);
        if (rel_path)
            path = rel_path;
    }

    /* FID */
    if (entry)
        fid = rbh_lu_fid_from_id(&entry->id);

    /* Parameters */
    if (params && sstack) {
        json_params = map2json(params, sstack);
        if (json_params) {
            json_str = json_dumps(json_params, JSON_COMPACT);
            json_decref(json_params);
        }
    }

    if (fid)
        printf("LogAction | fid=" DFID ", path=%s, params=%s\n",
               PFID(fid), path, json_str ? json_str : "{}");
    else
        printf("LogAction | fid=(NULL), path=%s, params=%s\n",
               path, json_str ? json_str : "{}");

    free(json_str);
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
        return rbh_lustre_log_entry(entry, &action->params.map,
                                    action->params.sstack);
    case RBH_ACTION_DELETE:
        return rbh_posix_delete_entry(mi_backend, entry);
    default:
        errno = ENOTSUP;
        return -1;
    }
}

