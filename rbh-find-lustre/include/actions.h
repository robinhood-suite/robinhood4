/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef RBH_FIND_LUSTRE_ACTION_H
#define RBH_FIND_LUSTRE_ACTION_H

#include <robinhood/fsentry.h>

int
fsentry_print_lustre_directive(char *output, int max_length,
                               const struct rbh_fsentry *fsentry,
                               const char *directive,
                               const char *backend);

#endif
