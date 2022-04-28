/* This file is part of rbh-find
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
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

#include <robinhood.h>
#include <robinhood/utils.h>

#include "rbh-find/actions.h"
#include "rbh-find/filters.h"
#include "rbh-find/parser.h"

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

    /** The file that should contain the results of an action, if specified */
    FILE *action_file;

    /**
     * Callback to prepare an action's execution
     *
     * @param ctx      find's context for this execution
     * @param index    the command line index of the action token
     * @param action   the type of action to prepare for
     *
     * @return         int corresponding to how many command line tokens were
     *                 consumed
     */
    int (*pre_action_callback)(struct find_context *ctx, const int index,
                               const enum action action);

    /**
     * Callback for executing an action
     *
     * @param ctx      find's context for this execution
     * @param action   the type of action to execute
     * @param fsentry  the fsentry to act on
     *
     * @return         1 if the action is ACT_COUNT, 0 otherwise.
     */
    int (*exec_action_callback)(struct find_context *ctx, enum action action,
                                struct rbh_fsentry *fsentry);

    /**
     * Callback to finish an action's execution
     *
     * @param ctx      find's context for this execution
     * @param index    the command line index of the token after the action
     * @param action   the type of action to prepare for
     * @param count    number of entries found with this action
     */
    void (*post_action_callback)(struct find_context *ctx, const int index,
                                 const enum action action, const size_t count);

    /**
     * Callback to parse a predicate in the command line
     *
     * @param ctx            find's context for this execution
     * @param arg_idx        index of the predicate to parse in the command line
     *
     * @return               a filter corresponding to the predicate
     */
    struct rbh_filter *(*parse_predicate_callback)(struct find_context *ctx,
                                                   int *arg_idx);
};

/**
 * Destroy and free the backends of a `struct find_context`
 *
 * @param ctx      find's context for this execution
 */
void
ctx_finish(struct find_context *ctx);

/**
 * Filter through every fsentries in a specific backend, executing the
 * requested action on each of them
 *
 * @param ctx            find's context for this execution
 * @param backend_index  index of the backend to go through
 * @param action         the type of action to execute
 * @param filter         the filter to apply to each fsentry
 * @param sorts          how the list of retrieved fsentries is sorted
 * @param sorts_count    how many fsentries to sort
 *
 * @return               the number of fsentries checked
 */
size_t
_find(struct find_context *ctx, int backend_index, enum action action,
      const struct rbh_filter *filter, const struct rbh_filter_sort *sorts,
      size_t sorts_count);

/**
 * Execute a find search corresponding to an action on each backend
 *
 * @param ctx            find's context for this execution
 * @param action         the type of action to execute
 * @param arg_idx        a pointer to the index of the action token in \p argv
 * @param filter         the filter to apply to each fsentry
 * @param sorts          list of criteria used to sort the list of fsentries
 * @param sorts_count    size of the sorts list
 */
void
find(struct find_context *ctx, enum action action, int *arg_idx,
     const struct rbh_filter *filter, const struct rbh_filter_sort *sorts,
     size_t sorts_count);

#endif
