/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_S3_INTERNALS_H
#define ROBINHOOD_S3_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/plugins/backend.h>
#include <robinhood/backend.h>
#include <robinhood/action.h>

struct item_data {
    int64_t length;
    int64_t current_id;
    char **list;
};

struct s3_iterator {
    struct rbh_mut_iterator iterator;
    struct rbh_sstack *values;
    struct item_data bkt_data;
    struct item_data obj_data;
};

int
rbh_s3_plugin_init(struct rbh_backend_plugin_init_arg *arg);

int
rbh_s3_backend_load_extensions(const struct rbh_backend_plugin *self,
                               void *backend, const char *type);

char *
get_next_object(struct s3_iterator *s3_iter);

void *
s3_iter_next(void *iterator);

/**
 * Release the memory associated with the iterator
 */
void
s3_iter_destroy(void *iterator);

/**
 * The following functions are implementations of the different callbacks of the
 * `rbh_pe_common_operations` structure. Their documentation is the same as the
 * one given for the structure's callbacks.
 */
enum rbh_parser_token
rbh_s3_check_valid_token(const char *token);

struct rbh_filter *
rbh_s3_build_filter(const char **argv, int argc, int *index,
                    bool *need_prefetch);

int
rbh_s3_delete_entry(struct rbh_backend *backend, struct rbh_fsentry *fsentry,
                    const struct rbh_delete_params *params);

int
rbh_s3_fill_entry_info(char *output, int max_length,
                       const struct rbh_fsentry *fsentry,
                       const char *format_string, size_t *index,
                       const char *backend);

enum known_directive
rbh_s3_fill_projection(struct rbh_filter_projection *projection,
                       const char *format_string, size_t *index);

void
rbh_s3_helper(__attribute__((unused)) const char *backend,
              __attribute__((unused)) struct rbh_config *config,
              char **predicate_helper, char **directive_helper);

#endif
