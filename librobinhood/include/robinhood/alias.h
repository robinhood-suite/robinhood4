/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_ALIAS_H
#define ROBINHOOD_ALIAS_H

void
rbh_apply_aliases(int *argc, char ***argv);

void
rbh_display_resolved_argv(char* name, int *argc, char ***argv);

#endif
