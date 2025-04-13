/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

#define backend_filter sqlite_backend_filter
#include "../../generic_branch.c"

struct sqlite_backend_branch {
    struct sqlite_backend sqlite;
    struct rbh_id id;
};

static const struct rbh_backend_operations SQLITE_BRANCH_BACKEND_OPS = {
    .branch  = sqlite_backend_branch,
    .root    = sqlite_branch_root,
    .update  = sqlite_backend_update,
    .filter  = generic_branch_backend_filter,
    .destroy = sqlite_backend_destroy,
};

static const struct rbh_backend SQLITE_BRANCH_BACKEND = {
    .id = RBH_BI_SQLITE,
    .name = RBH_SQLITE_BACKEND_NAME,
    .ops = &SQLITE_BRANCH_BACKEND_OPS,
};

struct rbh_backend *
sqlite_backend_branch(void *backend, const struct rbh_id *id, const char *path)
{
    struct sqlite_backend *sqlite = backend;
    struct sqlite_backend_branch *branch;
    size_t data_size;
    char *data;

    (void) path;

    data_size = id->size;
    branch = malloc(sizeof(*branch) + data_size);
    if (!branch)
        return NULL;

    data = (char *)branch + sizeof(*branch);

    rbh_id_copy(&branch->id, id, &data, &data_size);
    branch->sqlite.backend = SQLITE_BRANCH_BACKEND;

    if (!sqlite_backend_dup(sqlite, &branch->sqlite)) {
        free(branch);
        return NULL;
    }

    return &branch->sqlite.backend;
}

struct rbh_fsentry *
sqlite_branch_root(void *backend,
                   const struct rbh_filter_projection *projection)
{
    struct sqlite_backend_branch *branch = backend;
    const struct rbh_filter id_filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID,
            },
            .value = {
                .type = RBH_VT_BINARY,
                .binary = {
                    .data = branch->id.data,
                    .size = branch->id.size,
                },
            },
        },
    };
    const struct rbh_backend_operations *ops = branch->sqlite.backend.ops;
    struct rbh_fsentry *root;

    /* To avoid the infinite recursion root -> branch_filter -> root -> ... */
    branch->sqlite.backend.ops = &SQLITE_BACKEND_OPS;
    root = rbh_backend_filter_one(backend, &id_filter, projection);
    branch->sqlite.backend.ops = ops;
    return root;
}
