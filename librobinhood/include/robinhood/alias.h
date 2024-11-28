/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_ALIAS_H
#define ROBINHOOD_ALIAS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <sysexits.h>

#include "config.h"
#include "robinhood/stack.h"
#include "robinhood/sstack.h"

void
import_configuration_file(int *argc, char ***argv);

void
apply_aliases(int *argc, char ***argv);

#endif
