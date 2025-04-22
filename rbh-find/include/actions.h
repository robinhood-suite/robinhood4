/* SPDX-License-Identifer: LGPL-3.0-or-later */

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

int
find_pre_action(struct find_context *ctx, const int index,
                const enum action action);

int
find_exec_action(struct find_context *ctx,
                 size_t backend_index,
                 enum action action,
                 struct rbh_fsentry *fsentry);

void
find_post_action(struct find_context *ctx, const int index,
                 const enum action action, const size_t count);

#endif
