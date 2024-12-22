/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_LUSTRE_INTERNALS_H
#define RBH_LUSTRE_INTERNALS_H

#include <unistd.h>

#include "robinhood/backend.h"

struct entry_info;
struct rbh_value_pair;
struct rbh_sstack;

int
rbh_lustre_enrich(struct entry_info *einfo, struct rbh_value_pair *pairs,
                  struct rbh_sstack *values);

#endif
