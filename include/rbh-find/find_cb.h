/* This file is part of rbh-find
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_FIND_CB_H
#define RBH_FIND_FIND_CB_H

#include "rbh-find/core.h"

/**
 * Find pre_action function, see `pre_action_callback` in `struct
 * find_context` for more information.
 *
 * Called by rbh-find and implement GNU-find like behaviour.
 */
int
find_pre_action(struct find_context *ctx, const int index,
                const enum action action);

/**
 * Find exec_action function, see `exec_action_callback` in `struct
 * find_context` for more information.
 *
 * Called by rbh-find and implement GNU-find like behaviour.
 */
int
find_exec_action(struct find_context *ctx, enum action action,
                 struct rbh_fsentry *fsentry);

/**
 * Find post_action function, see `post_action_callback` in `struct
 * find_context` for more information.
 *
 * Called by rbh-find and implement GNU-find like behaviour.
 */
void
find_post_action(struct find_context *ctx, const int index,
                 const enum action action, const size_t count);

#endif
