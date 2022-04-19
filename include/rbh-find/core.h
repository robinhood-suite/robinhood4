/* This file is part of rbh-find
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_CORE_H
#define RBH_FIND_CORE_H

#include <error.h>
#include <stdlib.h>

#include <robinhood.h>

/**
 * Find's library context
 */
struct find_context {
    /** The backends to go through */
    size_t backend_count;
    struct rbh_backend **backends;

    /** The command-line arguments to parse */
    int argc;
    char **argv;

    /** If an action was already executed in this specific execution */
    bool action_done;
};

/**
 * Destroy and free the backends of a `struct find_context`
 *
 * @param ctx      find's context for this execution
 */
void
ctx_finish(struct find_context *ctx);

#endif
