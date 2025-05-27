/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_CORE_H
#define RBH_FIND_CORE_H

#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <robinhood/filter.h>
#include <robinhood.h>
#include <robinhood/utils.h>

#include "parser.h"

/**
 * Find's library context
 */
struct find_context {
    /** The backends to go through */
    size_t backend_count;
    struct rbh_backend **backends;
    const char **uris;

    /** The filters context which contains all the information related to the
     * filters.
     */
    struct filters_context f_ctx;

    /** The command-line arguments to parse */
    int argc;
    char **argv;

    /** If an action was already executed in this specific execution */
    bool action_done;

    /** The file that should contain the results of an action, if specified */
    FILE *action_file;

    /** The format string to use for printing the results of the command, if
     * specified
     */
    char *format_string;

    /** The command to execute on each entry and its arguments */
    char **exec_command;
};

/**
 * Destroy and free the backends of a `struct find_context`
 *
 * @param ctx      find's context for this execution
 */
void
ctx_finish(struct find_context *ctx);

/**
 * Execute a find search corresponding to an action on each backend
 *
 * @param ctx            find's context for this execution
 * @param action         the type of action to execute
 * @param arg_idx        a pointer to the index of the action token in \p argv
 * @param filter         the filter to apply to each fsentry
 * @param options        filter options and modifiers
 */
void
find(struct find_context *ctx, enum action action, int *arg_idx,
     const struct rbh_filter *filter, struct rbh_filter_options *options);

#endif
