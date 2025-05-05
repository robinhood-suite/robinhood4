/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_ACTION_H
#define RBH_FIND_ACTION_H

#include <stdio.h>

#include <robinhood/fsentry.h>

#include "parser.h"

struct find_context;

void
fsentry_print_ls_dils(FILE *file, const struct rbh_fsentry *fsentry);

void
fsentry_printf_format(struct find_context *ctx, size_t backend_index,
                      const struct rbh_fsentry *fsentry);

/**
 * Prepare an action's execution
 *
 * @param ctx      find's context for this execution
 * @param index    the command line index of the action token
 * @param action   the type of action to prepare for
 *
 * @return         int corresponding to how many command line tokens were
 *                 consumed
 */
int
find_pre_action(struct find_context *ctx, const int index,
                const enum action action);

/**
 * Execute an action
 *
 * @param ctx            find's context for this execution
 * @param backend_index  which backend is being queried
 * @param action         the type of action to execute
 * @param fsentry        the fsentry to act on
 *
 * @return         1 if the action is ACT_COUNT, 0 otherwise.
 */
int
find_exec_action(struct find_context *ctx,
                 size_t backend_index,
                 enum action action,
                 struct rbh_fsentry *fsentry);

/**
 * Finish an action's execution
 *
 * @param ctx      find's context for this execution
 * @param index    the command line index of the token after the action
 * @param action   the type of action to prepare for
 * @param count    number of entries found with this action
 */
void
find_post_action(struct find_context *ctx, const int index,
                 const enum action action, const size_t count);

#endif
