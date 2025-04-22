/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef RBH_FIND_ACTION_H
#define RBH_FIND_ACTION_H

#include <stdio.h>

#include <robinhood/fsentry.h>

struct find_context;

void
fsentry_print_ls_dils(FILE *file, const struct rbh_fsentry *fsentry);

void
fsentry_printf_format(struct find_context *ctx, size_t backend_index,
                      const struct rbh_fsentry *fsentry);

#endif
