/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 * alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_POLICYENGINE_PYTHON_H
#define RBH_POLICYENGINE_PYTHON_H

#include "robinhood/action.h"
#include "robinhood/backend.h"
#include "robinhood/fsentry.h"

int
rbh_pe_python_action(const struct rbh_action *action,
                     struct rbh_fsentry *entry,
                     struct rbh_backend *mi_backend);

#endif
