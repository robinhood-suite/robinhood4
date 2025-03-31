/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_PLUGIN_H
#define RBH_PLUGIN_H

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <robinhood/filter.h>

struct rbh_plugin {
    const char *name;
    uint64_t version;
};

struct rbh_plugin_extension {
    const char *super;
    const char *name;
    uint64_t version;
    uint64_t min_version;
    uint64_t max_version;
    enum rbh_parser_token (*check_valid_token)(const char *token);
    struct rbh_filter *(*build_filter)(const char **argv, int argc, int *index,
                                       bool *need_prefetch);
};

/**
 * Import a plugin
 *
 * @param name      the name of the plugin (from which the dynamic library name
 *                  is inferred)
 * @param symbol    the name of the actual symbol to dlsym() from the plugin's
 *                  dynamic library
 *
 * @return          the address where \p symbol is loaded into memory on
 *                  success, NULL on error and dlerror() can be used to
 *                  establish a diagnostic.
 */
void *
rbh_plugin_import(const char *name, const char *symbol);

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
rbh_plugin_extension_check_valid_token(
    const struct rbh_plugin_extension *extension,
    const char *token
)
{
    if (extension->check_valid_token)
        return extension->check_valid_token(token);

    errno = ENOTSUP;
    return RBH_TOKEN_UNKNOWN;
}

/**
 * Create a filter from the given command line arguments and position in that
 * command line
 *
 * @param extension      the extension to build a filter from
 * @param argv           the list of arguments given to the command
 * @param argc           the number of strings in \p argv
 * @param index          the argument currently being parsed, should be updated
 *                       by the extension if necessary to skip optionnal values
 * @param need_prefetch  boolean value to indicate if a filter needs to be
 *                       completed
 *
 * @return               a pointer to newly allocated struct rbh_filter on
 *                       success, NULL on error and errno is set appropriately
 */
static inline struct rbh_filter *
rbh_plugin_extension_build_filter(const struct rbh_plugin_extension *extension,
                                  const char **argv, int argc, int *index,
                                  bool *need_prefetch)
{
    if (extension->build_filter)
        return extension->build_filter(argv, argc, index, need_prefetch);

    errno = ENOTSUP;
    return NULL;
}

/*----------------------------------------------------------------------------*
 |                          Robinhood Plugin Version                          |
 *----------------------------------------------------------------------------*/

/* Version numbers are stored in 64 bits long unsigned integers as follows:
 *
 * 0                          27                         54        64
 * -----------------------------------------------------------------
 * |         revision         |           minor          |  major  |
 * -----------------------------------------------------------------
 *
 */

#define RPV_MAJOR_SHIFT 54
#define RPV_MINOR_SHIFT 27

#define RPV(major, minor, revision) \
    ((UINT64_C(major) << RPV_MAJOR_SHIFT) \
   + (UINT64_C(minor) << RPV_MINOR_SHIFT) \
   + UINT64_C(revision))

#define RPV_MAJOR_MASK      0xffc0000000000000
#define RPV_MINOR_MASK      0x003ffffff8000000
#define RPV_REVISION_MASK   0x0000000007ffffff

#define RPV_MAJOR(version) ((version) >> RPV_MAJOR_SHIFT)
#define RPV_MINOR(version) (((version) & RPV_MINOR_MASK) >> RPV_MINOR_SHIFT)
#define RPV_REVISION(version) ((version) & RPV_REVISION_MASK)

/**
 * Binary capabilities indicator to identify the presence of options: filter,
 * update and branch in the backends.
 */
#define RBH_FILTER_OPS 0b1000
#define RBH_UPDATE_OPS 0b0100
#define RBH_BRANCH_OPS 0b0010
#define RBH_SYNC_OPS 0b0001
#define RBH_EMPTY_OPS 0b0000

#endif
