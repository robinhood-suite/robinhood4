/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood/action.h>
#include <robinhood/backend.h>
#include <robinhood.h>
#include "lu_fid.h"
#include "../../posix_internals.h"

static int
rbh_lustre_log_entry(struct rbh_fsentry *entry,
                     const struct rbh_value_map *params,
                     struct rbh_sstack *sstack)
{
    char fid_str[LU_FID_STRING_SIZE];
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
    if (entry) {
        const struct lu_fid *fid = rbh_lu_fid_from_id(&entry->id);
        fid_to_str(fid_str, sizeof(fid_str), fid);
    } else {
        snprintf(fid_str, sizeof(fid_str), "(NULL)");
    }

    /* Parameters */
    if (params && sstack) {
        json_params = map2json(params, sstack);
        if (json_params) {
            json_str = json_dumps(json_params, JSON_COMPACT);
            json_decref(json_params);
        }
    }

    printf("LogAction | fid=%s, path=%s, params=%s\n", fid_str, path,
           json_str ? json_str : "{}");

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

