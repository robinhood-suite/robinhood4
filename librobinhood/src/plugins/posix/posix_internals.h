/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_POSIX_INTERNALS_H
#define ROBINHOOD_POSIX_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/id.h>
#include <robinhood/backends/posix_extension.h>
#include <robinhood/backend.h>

struct rbh_mut_iterator *
fts_iter_new(const char *root, const char *entry, int statx_sync_type);

int
fts_iter_root_setup(struct posix_iterator *_iter);

bool
rbh_posix_iter_is_fts(struct posix_iterator *iter);

int
load_posix_extensions(const struct rbh_plugin *self,
                      struct posix_backend *posix,
                      const char *type,
                      struct rbh_config *config);

/**
 * Show POSIX and its extensions helper, detailling specific predicates and
 * directives.
 *
 * @param backend            the backend specified by user, which may be
 *                           present in \p config, which POSIX may use to fetch
 *                           additionnal helper
 * @param config             the config to use to determine which other
 *                           extensions' helpers should be fetched
 * @param predicate_helper   the output helper for predicates
 * @param directive_helper   the output helper for directives
 *
 * @return                   POSIX and its extension's helper for predicates
 *                           and directives
 */
void
rbh_posix_helper(const char *backend, struct rbh_config *config,
                 char **predicate_helper, char **directive_helper);

/**
 * Check the given token corresponds to a predicate or action known by POSIX
 *
 * @param token   a string that represents a token POSIX should identify
 *
 * @return        RBH_TOKEN_PREDICATE if the token is a predicate
 *                RBH_TOKEN_ACTION if the token is an action
 *                RBH_TOKEN_UNKNOWN if the token is not valid
 *                RBH_TOKEN_ERROR if an error occur and errno is set
 *                appropriately
 */
enum rbh_parser_token
rbh_posix_check_valid_token(const char *token);

/**
 * Create a filter from the given command line arguments and position in that
 * command line
 *
 * @param argv           the list of arguments given to the command
 * @param argc           the number of strings in \p argv
 * @param index          the argument currently being parsed, should be updated
 *                       if necessary to skip optionnal values
 * @param need_prefetch  boolean value set by posix to indicate if a filter
 *                       needs to be completed
 *
 * @return               a pointer to newly allocated struct rbh_filter on
 *                       success, NULL on error and errno is set appropriately
 */
struct rbh_filter *
rbh_posix_build_filter(const char **argv, int argc, int *index,
                       bool *need_prefetch);

/**
 * Fill information about an entry according to a given directive into a buffer
 *
 * @param output         an array in which the information can be printed
 *                       according to \p directive. Size of the array should be
 *                       \p max_length
 * @param max_length     size of the \p output array
 * @param fsentry        fsentry to print
 * @param directive      which information about \p fsentry to print
 * @param backend        the backend to print (XXX: may not be necessary)
 *
 * @return               the number of characters written to \p output, -1 on
 *                       error
 */
int
rbh_posix_fill_entry_info(char *output, int max_length,
                          const struct rbh_fsentry *fsentry,
                          const char *directive, const char *backend);

/**
 * Delete an entry from the plugin
 *
 * @param fsentry        the entry to delete
 *
 * @return               0 on success, -1 on error and errno is set
 */
int
rbh_posix_delete_entry(struct rbh_fsentry *fsentry);

#endif
