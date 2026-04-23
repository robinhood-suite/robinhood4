/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_ACT
#define RBH_ACT

#include <stddef.h>

#include "robinhood/sstack.h"
#include "robinhood/value.h"

struct filters_context;
struct rbh_fsentry;

// Indicator for directives that are not supported by GNU's find
#define RBH_NON_STANDARD_DIRECTIVE 'R'

/**
 * Different output of the `fill_projection` function of a plugin/extension
 * common ops.
 */
enum known_directive {
    RBH_DIRECTIVE_ERROR,
    RBH_DIRECTIVE_KNOWN,
    RBH_DIRECTIVE_UNKNOWN
};

/**
 * Types of actions supported by the policy engine.
 */
enum rbh_action_type {
    RBH_ACTION_UNSET,
    RBH_ACTION_DELETE,
    RBH_ACTION_CMD,
    RBH_ACTION_LOG,
    RBH_ACTION_PYTHON,
    RBH_ACTION_UNKNOWN
};

enum rbh_delete_action_return {
    RBH_DELETE_OK,
    RBH_DELETE_OK_WITH_PARENTS,
    RBH_DELETE_ERROR
};

struct rbh_delete_params {
    bool remove_empty_parent;
    const char *remove_parents_below;
};

struct rbh_log_params {
    const char *format;
};

/**
 * Parsed representation of an action.
 *
 * The action type identifies the operation to perform, and the value field
 * stores the raw action string associated with the action.
 */
struct rbh_action {
    enum rbh_action_type type;
    const char *value;
    union {
        struct rbh_value_map generic;
        struct rbh_delete_params delete;
        struct rbh_log_params log;
    } params;
};

const char *
action2string(enum rbh_action_type type);

/**
 * Parse YAML parameters into a value_map.
 *
 * Parses YAML-formatted parameter string and fills the provided value_map.
 * Memory is allocated using the global context from serialization.c and
 * persists until program exit.
 *
 * @param parameters  YAML-formatted parameter string (e.g. "key: value")
 * @param map         pointer to the value_map to fill
 *
 * @return            true on success, false on error
 */
bool
rbh_action_parameters2value_map(const char *parameters,
                                struct rbh_value_map *map);

/**
 * Execute a pre-split command with "{}" placeholders replaced by a path.
 *
 * Each element of \p argv that contains "{}" is expanded with \p path.
 * The command is executed synchronously via fork(2) + execvp(3).
 *
 * @param argv  a NULL-terminated argument array
 * @param path  the path to substitute for "{}"
 *
 * @return      the exit status of the child on success, -1 on error
 */
int
rbh_action_exec_argv(const char **argv, const char *path);

/**
 * Execute an external command with "{}" placeholders replaced by a path.
 *
 * The command string is split into tokens using wordexp(3), then passed
 * to rbh_action_exec_argv().
 *
 * @param cmd_str  the command template (e.g. "archive_tool --path {}")
 * @param path     the path to substitute for "{}"
 *
 * @return         the exit status of the child on success, -1 on error
 */
int
rbh_action_exec_command(const char *cmd_str, const char *path);

/**
 * Format a filesystem entry according to a printf-like format string.
 *
 * Directives are resolved through registered plugins/extensions from
 * \p f_ctx (via fill_entry_info callbacks).
 *
 * @param format_string  format string containing directives (e.g. "%p\n")
 * @param f_ctx          filters context containing plugin/extension metadata
 * @param fsentry        entry to format
 * @param backend        backend identifier passed to providers (may be NULL)
 * @param output         output buffer
 * @param output_size    size of \p output
 *
 * @return               number of chars written (excluding trailing '\0')
 *                       -1 on error with errno set
 */
int
rbh_action_format_fsentry(const char *format_string,
                          const struct filters_context *f_ctx,
                          const struct rbh_fsentry *fsentry,
                          const char *backend,
                          char *output,
                          size_t output_size);
#endif
