/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_PE
#define RBH_PE

#include "robinhood/filters/core.h"
#include "robinhood/fsentry.h"

struct rbh_mut_iterator *
rbh_collect_fsentry(const char *uri, struct rbh_filter *filter);

#endif

