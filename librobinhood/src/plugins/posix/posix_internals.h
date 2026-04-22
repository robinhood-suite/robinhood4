/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
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
fts_iter_new(struct rbh_metadata *metadata, const char *root, const char *entry,
             int statx_sync_type);

int
fts_iter_root_setup(struct posix_iterator *_iter);

bool
rbh_posix_iter_is_fts(struct posix_iterator *iter);


int
rbh_posix_backend_load_extensions(const struct rbh_backend_plugin *self,
                                  void *backend, const char *type);

/**
 * The following functions are implementations of the different callbacks of the
 * `rbh_pe_common_operations` structure. Their documentation is the same as the
 * one given for the structure's callbacks.
 */
void
rbh_posix_helper(const char *backend, struct rbh_config *config,
                 char **predicate_helper, char **directive_helper);

enum rbh_parser_token
rbh_posix_check_valid_token(const char *token);

struct rbh_filter_field
rbh_posix_sort2field(const char *attribute);

struct rbh_filter *
rbh_posix_build_filter(const char **argv, int argc, int *index,
                       bool *need_prefetch);

int
rbh_posix_delete_entry(struct rbh_backend *backend,
                       struct rbh_fsentry *fsentry,
                       const struct rbh_delete_params *params);

int
rbh_posix_fill_entry_info(char *output, int max_length,
                          const struct rbh_fsentry *fsentry,
                          const char *format_string, size_t *index,
                          const char *backend);

enum known_directive
rbh_posix_fill_projection(struct rbh_filter_projection *projection,
                          const char *format_string, size_t *index);

#endif
