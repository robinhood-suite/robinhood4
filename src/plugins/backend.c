/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "robinhood/plugins/backend.h"

static char *
strtoupper(char *string)
{
    for (char *c = string; *c != '\0'; c++)
        *c = toupper(*c);

    return string;
}

char *
rbh_backend_plugin_symbol(const char *name)
{
    char *symbol;

    if (asprintf(&symbol, "_RBH_%s_BACKEND_PLUGIN", name) < 0) {
        errno = ENOMEM;
        return NULL;
    }
    return strtoupper(symbol);
}

const struct rbh_backend_plugin *
rbh_backend_plugin_import(const char *name)
{
    const struct rbh_backend_plugin *plugin;
    int save_errno = errno;
    char *symbol;

    symbol = rbh_backend_plugin_symbol(name);
    if (symbol == NULL)
        return NULL;

    plugin = rbh_plugin_import(name, symbol);
    free(symbol);
    errno = save_errno;

    return plugin;
}
