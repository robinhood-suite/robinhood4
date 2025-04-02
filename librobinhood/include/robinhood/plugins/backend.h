/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
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
#include "robinhood/fsentry.h"
#include "robinhood/plugin.h"

struct rbh_backend_plugin {
    struct rbh_plugin plugin;
    const struct rbh_backend_plugin_operations *ops;
    const struct rbh_pe_common_operations *common_ops;
    const uint8_t capabilities;
    const uint8_t info;
};

struct rbh_backend_plugin_operations {
    struct rbh_backend *(*new)(const struct rbh_backend_plugin *self,
                               const char *type,
                               const char *fsname,
                               struct rbh_config *config);
    enum rbh_parser_token (*check_valid_token)(const char *token);
    struct rbh_filter *(*build_filter)(const char **argv, int argc, int *index,
                                       bool *need_prefetch);
    int (*fill_entry_info)(char *output, int max_length,
                           const struct rbh_fsentry *fsentry,
                           const char *directive, const char *backend);
    int (*delete_entry)(struct rbh_fsentry *fsentry);
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
                       struct rbh_config *config)
{
    return plugin->ops->new(plugin, type, fsname, config);
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
 * @param plugin         the plugin to build a filter from
 * @param argv           the list of arguments given to the command
 * @param argc           the number of strings in \p argv
 * @param index          the argument currently being parsed, should be updated
 *                       by the plugin if necessary to skip optionnal values
 * @param need_prefetch  boolean value set by posix to indicate if a filter
 *                       needs to be completed
 *
 * @return               a pointer to newly allocated struct rbh_filter on
 *                       success, NULL on error and errno is set appropriately
 */
static inline struct rbh_filter *
rbh_plugin_build_filter(const struct rbh_backend_plugin *plugin,
                        const char **argv, int argc, int *index,
                        bool *need_prefetch)
{
    if (plugin->ops->build_filter)
        return plugin->ops->build_filter(argv, argc, index, need_prefetch);

    errno = ENOTSUP;
    return NULL;
}

/**
 * Fill information about an entry according to a given directive into a buffer
 *
 * @param plugin         the plugin that will print info
 * @param output         an array in which the information can be printed
 *                       according to \p directive. Size of the array should be
 *                       \p max_length
 * @param max_length     size of the \p output array
 * @param fsentry        fsentry to print
 * @param directive      which information about \p fsentry to print
 * @param backend        the backend to print (XXX: may not be necessary)
 *
 * @return               the number of characters written to \p output on
 *                       success
 *                       0 if the plugin doesn't know the directive requested
 *                       -1 on error
 */
static inline int
rbh_plugin_fill_entry_info(const struct rbh_backend_plugin *plugin,
                           char *output, int max_length,
                           const struct rbh_fsentry *fsentry,
                           const char *directive, const char *backend)
{
    if (plugin->ops->fill_entry_info)
        return plugin->ops->fill_entry_info(output, max_length, fsentry,
                                            directive, backend);

    errno = ENOTSUP;
    return -1;
}

/**
 * Delete an entry from the plugin
 *
 * @param plugin         the plugin to delete an entry from
 * @param fsentry        the entry to delete
 *
 * @return               0 on success, -1 on error and errno is set
 */
static inline int
rbh_plugin_delete_entry(const struct rbh_backend_plugin *plugin,
                        struct rbh_fsentry *fsentry)
{
    if (plugin->ops->delete_entry)
        return plugin->ops->delete_entry(fsentry);

    errno = ENOTSUP;
    return -1;
}

/**
 * Check the given token corresponds to a predicate or action known by the
 * extension.
 *
 * @param extension  the extension to check the token with
 * @param token      a string that represents a token the extension should
 *                   identify
 *
 * @return           RBH_TOKEN_PREDICATE if the token is a predicate
 *                   RBH_TOKEN_ACTION if the token is an action
 *                   RBH_TOKEN_UNKNOWN if the token is not valid
 *                   RBH_TOKEN_ERROR if an error occur
 */
static inline enum rbh_parser_token
rbh_extension_check_valid_token(const struct rbh_plugin_extension *extension,
                                const char *token)
{
    if (extension->check_valid_token)
        return extension->check_valid_token(token);

    errno = ENOTSUP;
    return RBH_TOKEN_ERROR;
}

/**
 * Create a filter from the given command line arguments and position in that
 * command line
 *
 * @param extension      the extension to build a filter from
 * @param argv           the list of arguments given to the command
 * @param argc           the number of strings in \p argv
 * @param index          the argument currently being parsed, should be updated
 *                       by the plugin if necessary to skip optionnal values
 * @param need_prefetch  boolean value set by posix to indicate if a filter
 *                       needs to be completed
 *
 * @return               a pointer to newly allocated struct rbh_filter on
 *                       success, NULL on error and errno is set appropriately
 */
static inline struct rbh_filter *
rbh_extension_build_filter(const struct rbh_plugin_extension *extension,
                           const char **argv, int argc, int *index,
                           bool *need_prefetch)
{
    if (extension->build_filter)
        return extension->build_filter(argv, argc, index, need_prefetch);

    errno = ENOTSUP;
    return NULL;
}

/**
 * Fill information about an entry according to a given directive into a buffer
 *
 * @param extension      the extension that will print entry info
 * @param output         an array in which the information can be printed
 *                       according to \p directive. Size of the array should be
 *                       \p max_length
 * @param max_length     size of the \p output array
 * @param fsentry        fsentry to print
 * @param directive      which information about \p fsentry to print
 * @param backend        the backend to print (XXX: may not be necessary)
 *
 * @return               the number of characters written to \p output on
 *                       success
 *                       0 if the plugin doesn't know the directive requested
 *                       -1 on error
 */
static inline int
rbh_extension_fill_entry_info(const struct rbh_plugin_extension *extension,
                              char *output, int max_length,
                              const struct rbh_fsentry *fsentry,
                              const char *directive, const char *backend)
{
    if (extension->fill_entry_info)
        return extension->fill_entry_info(output, max_length, fsentry,
                                          directive, backend);

    errno = ENOTSUP;
    return -1;
}

/*
 * Delete an entry from the extension
 *
 * @param extension      the extension to delete an entry from
 * @param fsentry        the entry to delete
 *
 * @return               0 on success, -1 on error and errno is set
 */
static inline int
rbh_extension_delete_entry(const struct rbh_plugin_extension *extension,
                           struct rbh_fsentry *fsentry)
{
    if (extension->delete_entry)
        return extension->delete_entry(fsentry);

    errno = ENOTSUP;
    return -1;
}

#endif
