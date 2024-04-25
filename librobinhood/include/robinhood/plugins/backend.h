/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_BACKEND_PLUGIN_H
#define ROBINHOOD_BACKEND_PLUGIN_H

#include <assert.h>
#include <error.h>
#include <stdlib.h>
#include <errno.h>

#include "robinhood/plugin.h"
#include "robinhood/backend.h"

struct rbh_backend_plugin {
    struct rbh_plugin plugin;
    const struct rbh_backend_plugin_operations *ops;
};

struct rbh_backend_plugin_operations {
    struct rbh_backend *(*new)(const char *fsname);
    void (*destroy)();
};

/**
 * Macro plugins should use to name the struct rbh_backend_plugin they export
 *
 * @param name  the name of the plugin in capital letters
 *
 * @return      a symbol to export
 *
 * Failure to use an all cap \p name will result in rbh_backend_plugin_symbol()
 * returning the wrong symbol and ultimately in your backend plugin being
 * impossible to use.
 */
#define RBH_BACKEND_PLUGIN_SYMBOL(name) _RBH_ ## name ## _BACKEND_PLUGIN

/**
 * Build the name of the symbol a robinhood backend plugin should export
 *
 * @param name      the name of the plugin
 *
 * @return          a pointer to a newly allocated string that represent the
 *                  symbol the robinhood backend \p name should export, or NULL
 *                  on error with errno set to indicate the cause of the error.
 *
 * @error ENOMEM    there was not enough memory available
 */
char *
rbh_backend_plugin_symbol(const char *name);

/**
 * Import a backend plugin from its name
 *
 * @param name      the name of the plugin
 *
 * @return          a constant pointer to the backend plugin named \p name on
 *                  success, NULL on error in which case either errno is set
 *                  appropriately, or dlerror() can be used to establish a
 *                  diagnostic.
 *
 * @error ENOMEM    there was not enough memory available
 */
const struct rbh_backend_plugin *
rbh_backend_plugin_import(const char *name);

/**
 * Create a backend from a backend plugin
 *
 * @param plugin    the plugin to create a backend from
 * @param fsname    a string that identifies which filesystem to load a backend
 *                  for
 *
 * @return          a pointer to newly allocated struct rbh_backend on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 * @error TODO
 */
static inline struct rbh_backend *
rbh_backend_plugin_new(const struct rbh_backend_plugin *plugin, 
                       const char *fsname)
{
    return plugin->ops->new(fsname);
}

/**
 * Clear the memory associated with a plugin
 *
 * @param name   the name of the plugin
 *
 */
static inline void
rbh_backend_plugin_destroy(const char *name)
{
    const struct rbh_backend_plugin *plugin;

    plugin = rbh_backend_plugin_import(name);
    if (plugin == NULL)
        error(EXIT_FAILURE, errno, "failed to load robinhood plugin %s", name);

    if (plugin->ops->destroy)
        plugin->ops->destroy();
}

#endif
