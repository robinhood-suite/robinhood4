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

#include <robinhood.h>
#include <robinhood/utils.h>

#include "rbh-find/actions.h"
#include "rbh-find/filters.h"
#include "rbh-find/parser.h"

struct rbh_plugin_or_extension {
    bool is_plugin;
    union {
        const struct rbh_backend_plugin *plugin;
        const struct rbh_plugin_extension *extension;
    };
};

static inline const struct rbh_pe_common_operations *
get_common_operations(struct rbh_plugin_or_extension *pe)
{
    if (pe->is_plugin)
        return pe->plugin->common_ops;
    else
        return pe->extension->common_ops;
}

/**
 * Find's library context
 */
struct find_context {
    /** The backends to go through */
    size_t backend_count;
    struct rbh_backend **backends;
    const char **uris;

    /** The type of information stored in the backends */
    struct rbh_plugin_or_extension *info_pe;
    size_t info_pe_count;

    /** The command-line arguments to parse */
    int argc;
    char **argv;

    /** If an action was already executed in this specific execution */
    bool action_done;

    /** If a filter need to be completed by an information of the backend */
    bool need_prefetch;

    /** The file that should contain the results of an action, if specified */
    FILE *action_file;

    /** The format string to use for printing the results of the command, if
     * specified
     */
    char *format_string;

    /** The command to execute on each entry and its arguments */
    char **exec_command;

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
     * @param ctx            find's context for this execution
     * @param backend_index  which backend is being queried
     * @param action         the type of action to execute
     * @param fsentry        the fsentry to act on
     *
     * @return         1 if the action is ACT_COUNT, 0 otherwise.
     */
    int (*exec_action_callback)(struct find_context *ctx,
                                size_t backend_index,
                                enum action action,
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
};

/**
 * Destroy and free the backends of a `struct find_context`
 *
 * @param ctx      find's context for this execution
 */
void
ctx_finish(struct find_context *ctx);

/**
 * str2command_line_token - command line token classifier
 *
 * @param ctx              find's context for this execution
 * @param string           the string to classify
 * @param pe_index         the index of the plugin or extension that recognizes
 *                         \p string
 *
 * @return                 the command_line_token that represents \p string
 *
 * \p string does not need to be a valid token
 */
enum command_line_token
str2command_line_token(struct find_context *ctx, const char *string,
                       int *pe_index);

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

/**
 * parse_expression - parse a find expression (predicates / operators / actions)
 *
 * @param ctx           find's context for this execution
 * @param arg_idx       a pointer to the index of argv to start parsing at
 * @param _filter       a filter (the part of the cli already parsed)
 * @param sorts         an array of filtering options
 * @param sorts_count   the size of \p sorts
 *
 * @return              a filter that represents the parsed expression
 *
 * Note this function is recursive and will call find() itself if it parses an
 * action
 */
struct rbh_filter *
parse_expression(struct find_context *ctx, int *arg_idx,
                 const struct rbh_filter *_filter,
                 struct rbh_filter_sort **sorts, size_t *sorts_count);

/**
 * Count the number of arguments to rbh-find like commands before reaching
 * the first URI. This function is useful when rbh-find like commands want
 * to count the number of arguments that are used before predicates and URIs.
 */
int
rbh_find_count_args_before_uri(int argc, char **argv);

#endif
