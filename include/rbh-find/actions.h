/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef RBH_FIND_ACTION_H
#define RBH_FIND_ACTION_H

#include <stdio.h>

#include <robinhood/fsentry.h>

void
fsentry_print_ls_dils(FILE *file, const struct rbh_fsentry *fsentry);

const char *
fsentry_path(const struct rbh_fsentry *fsentry);

#endif
