/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ALIAS_H
#define ALIAS_H

#include "config.h"

struct alias_entry {
    const char *name;
    struct rbh_value *value;
};
int
apply_alias(const char *alias_name, int *new_argc, char **new_argv[], int *argc,
            char *argv[]);
int
load_aliases_from_config(void);

void
rbh_aliases_free(void);

#endif
