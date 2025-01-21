/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "robinhood/plugin.h"
#include "robinhood/plugins/backend.h"

static char *
strtoupper(char *string)
{
    for (char *c = string; *c != '\0'; c++)
        *c = toupper(*c);

    return string;
}

static char *
remove_non_alnum(char *string)
{
    for (char *c = string; *c != '\0'; c++) {
        if (!isalnum(*c) && *c != '_')
            *c = '_';
    }
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
    return remove_non_alnum(strtoupper(symbol));
}

char *
rbh_plugin_extension_symbol(const char *super, const char *name)
{
    char *symbol;

    if (asprintf(&symbol, "_RBH_%s_%s_PLUGIN_EXTENSION", super, name) < 0) {
        errno = ENOMEM;
        return NULL;
    }
    return remove_non_alnum(strtoupper(symbol));
}

const struct rbh_backend_plugin *
rbh_backend_plugin_import(const char *name)
{
    const struct rbh_backend_plugin *plugin;
    int save_errno;
    char *symbol;

    symbol = rbh_backend_plugin_symbol(name);
    if (symbol == NULL)
        return NULL;

    plugin = rbh_plugin_import(name, symbol);
    save_errno = errno;
    free(symbol);
    errno = save_errno;

    return plugin;
}

static int
rbh_extension_libname(const char *super, const char *name, char *buf,
                       size_t len)
{
    int rc;

    rc = snprintf(buf, len, "%s-%s-ext", super, name);
    if (rc < 0 || rc >= len) {
        errno = ERANGE;
        return -1;
    }

    return 0;
}

static bool
extension_is_compatible(const struct rbh_plugin *super,
                        const struct rbh_plugin_extension *ext)
{
    return super->version >= ext->min_version &&
        super->version <= ext->max_version;
}

const struct rbh_plugin_extension *
rbh_plugin_load_extension(const struct rbh_plugin *super, const char *name)
{
    const struct rbh_plugin_extension *extension = NULL;
    char extension_name[PATH_MAX];
    int save_errno = errno;
    char *symbol;
    int rc;

    rc = rbh_extension_libname(super->name, name, extension_name,
                               sizeof(extension_name));
    if (rc == -1)
        return NULL;

    symbol = rbh_plugin_extension_symbol(super->name, name);
    if (!symbol)
        return NULL;

    extension = rbh_plugin_import(extension_name, symbol);
    if (!extension)
        goto free_symbol;

    if (strcmp(super->name, extension->super)) {
        save_errno = EINVAL;
        extension = NULL;
        goto free_symbol;
    }

    if (!extension_is_compatible(super, extension)) {
        save_errno = ERANGE;
        extension = NULL;
        goto free_symbol;
    }

free_symbol:
    free(symbol);
    errno = save_errno;

    return extension;
}
