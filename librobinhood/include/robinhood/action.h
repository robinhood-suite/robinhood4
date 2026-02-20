/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_ACT
#define RBH_ACT

#include "robinhood/sstack.h"
#include "robinhood/value.h"

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

struct rbh_action_parameters {
    struct rbh_value_map map;
    struct rbh_sstack *sstack;
    bool initialized;
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
    struct rbh_action_parameters params;
};

bool
rbh_action_parameters2value_map(const char *parameters,
                                struct rbh_value_map *map,
                                struct rbh_sstack *sstack);

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

#endif
