/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_ACT
#define RBH_ACT

/**
 * Types of actions supported by the policy engine.
 */
enum rbh_action_type {
    RBH_ACTION_UNSET,
    RBH_ACTION_DELETE,
    RBH_ACTION_CMD,
    RBH_ACTION_PRINT,
    RBH_ACTION_PYTHON,
    RBH_ACTION_UNKNOWN
};

/**
 * Parsed representation of an action.
 *
 * The action type identifies the operation to perform, and the value field
 * stores the raw action string associated with the action.
 */
struct rbh_action {
    enum rbh_action_type type;
    const char *parameters;
    const char *value;
};

#endif
