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
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include "robinhood/backend.h"
#include "robinhood/config.h"
#include "robinhood/filter.h"
#include "robinhood/plugin.h"

struct rbh_backend_plugin {
    struct rbh_plugin plugin;
    const struct rbh_backend_plugin_operations *ops;
    const uint8_t capabilities;
    const uint8_t info;
};

struct rbh_backend_plugin_operations {
    struct rbh_backend *(*new)(const char *fsname, struct rbh_config *config);
    enum rbh_parser_token (*check_valid_token)(const char *token);
    struct rbh_filter *(*build_filter)(const char **argv, int argc, int *index,
                                       bool need_prefetch);
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
                       const char *fsname, struct rbh_config *config)
{
    return plugin->ops->new(fsname, config);
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

/**
 * Check the given token corresponds to a predicate or action known by the
 * plugin.
 *
 * @param plugin    the plugin to check the token with
 * @param token     a string that represents a token the plugin should identify
 *
 * @return          RBH_TOKEN_PREDICATE if the token is a predicate
 *                  RBH_TOKEN_ACTION if the token is an action
 *                  RBH_TOKEN_UNKNOWN if the token is not valid
 *                  RBH_TOKEN_ERROR if an error occur
 */

static inline enum rbh_parser_token
rbh_plugin_check_valid_token(const struct rbh_backend_plugin *plugin,
                             const char *token)
{
    if (plugin->ops->check_valid_token)
        return plugin->ops->check_valid_token(token);

    errno = ENOTSUP;
    return RBH_TOKEN_ERROR;
}

/**
 * Create a filter from the given command line arguments and position in that
 * command line
 *
 * @param plugin        the plugin to build a filter from
 * @param argv          the list of arguments given to the command
 * @param argc          the number of strings in \p argv
 * @param index         the argument currently being parsed, should be updated
 *                      by
 *                      the plugin if necessary to skip optionnal values
 * @param need_prefetch boolean value set by the plugin to indicate if a filter
 *                      needs to be complete
 *
 * @return          a pointer to newly allocated struct rbh_filter on success,
 *                  NULL on error and errno is set appropriately
 */

static inline struct rbh_filter *
rbh_plugin_build_filter(const struct rbh_backend_plugin *plugin,
                        const char **argv, int argc, int *index,
                        bool need_prefetch)
{
    if (plugin->ops->build_filter)
        return plugin->ops->build_filter(argv, argc, index, need_prefetch);

    errno = ENOTSUP;
    return NULL;
}

#endif
