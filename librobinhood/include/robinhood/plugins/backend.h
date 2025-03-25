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

#include "robinhood/backend.h"
#include "robinhood/config.h"
#include "robinhood/plugin.h"

struct rbh_backend_plugin {
    struct rbh_plugin plugin;
    const struct rbh_backend_plugin_operations *ops;
    const uint8_t capabilities;
    const uint8_t info;
};

struct rbh_backend_plugin_operations {
    struct rbh_backend *(*new)(const struct rbh_backend_plugin *self,
                               const char *type,
                               const char *fsname,
                               struct rbh_config *config,
                               bool read_only);
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
 * Macro plugins should use to name the struct rbh_plugin_extension exported
 * by backend extensions
 *
 * @param super the name of the plugin that is extended in capital letters
 * @param name  the name of the extension
 *
 * @return      a symbol to export
 *
 * Failure to use an all cap \p super and \p name will result in
 * rbh_plugin_extension_symbol() returning the wrong symbol and ultimately in
 * your extension plugin being impossible to use.
 */
#define RBH_BACKEND_EXTENDS(super, name) \
    _RBH_ ## super ## _ ## name ## _PLUGIN_EXTENSION

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
 * Build the name of the symbol that should be exported by the extension
 * \p name to the robinhood backend \p super
 *
 * @param super     the name of the plugin that is extented
 * @param name      the name of the extension
 *
 * @return          a pointer to a newly allocated string that represent the
 *                  extension's symbol
 *
 * @error ENOMEM    there was not enough memory available
 */
char *
rbh_plugin_extension_symbol(const char *super, const char *name);

/**
 * Import the backend extension \p name that extends the robinhood plugin
 * \p super. The rbh_plugin_extension returned by this function can be embedded
 * in a backend specific structure that will only be known by the backend and
 * the extension itself. librobinhood will treat this pointer as a simple
 * struct rbh_plugin_extension much like the way rbh_iterator works.
 *
 *
 * @param super     the plugin that is extended by \p name
 * @param name      the name of the extension
 *
 * @return          a constant pointer to the extension on success,
 *                  NULL on error in which case either errno is set
 *                  appropriately, or dlerror() can be used to establish a
 *                  diagnostic.
 *
 * @error ENOMEM    there was not enough memory available
 * @error ERANGE    the version of \p super is not supported by the extension
 * @error EINVAL    \p name does not extend \p super
 */
const struct rbh_plugin_extension *
rbh_plugin_load_extension(const struct rbh_plugin *super, const char *name);

/**
 * Create a backend from a backend plugin
 *
 * @param plugin    the plugin to create a backend from
 * @param type      if not NULL, it is the name given in the URI. If NULL, the
 *                  name in the URI was the same as the plugin name
 * @param fsname    a string that identifies which filesystem to load a backend
 *                  for
 * @param read_only whether we intend to open the backend in read_only mode or
 *                  read/write
 *
 * @return          a pointer to newly allocated struct rbh_backend on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 * @error TODO
 *
 */
static inline struct rbh_backend *
rbh_backend_plugin_new(const struct rbh_backend_plugin *plugin,
                       const char *type,
                       const char *fsname,
                       struct rbh_config *config,
                       bool read_only)
{
    return plugin->ops->new(plugin, type, fsname, config, read_only);
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
    if (plugin == NULL) {
        if (errno == RBH_BACKEND_ERROR)
            error(EXIT_FAILURE, 0, "%s", rbh_backend_error);

        error(EXIT_FAILURE, errno, "failed to load robinhood plugin %s", name);
    }

    if (plugin->ops->destroy)
        plugin->ops->destroy();
}

#endif
