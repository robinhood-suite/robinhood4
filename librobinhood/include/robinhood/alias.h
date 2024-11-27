/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ALIAS_H
#define ALIAS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <sysexits.h>
#include "config.h"

void
handle_config_option(int argc, char *argv[], int index);

void
import_configuration_file(int *argc, char ***argv);

#endif
