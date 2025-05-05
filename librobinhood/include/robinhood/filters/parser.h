/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_PARSER_H
#define ROBINHOOD_PARSER_H

#include "robinhood/filters.h"

enum command_line_token {
    CLT_URI,
    CLT_AND,
    CLT_OR,
    CLT_NOT,
    CLT_PARENTHESIS_OPEN,
    CLT_PARENTHESIS_CLOSE,
    CLT_PREDICATE,
    CLT_ACTION,
    CLT_SORT,
    CLT_RSORT,
};

/**
 * str2command_line_token - command line token classifier
 *
 * @param ctx              filters context
 * @param string           the string to classify
 * @param pe_index         the index of the plugin or extension that recognizes
 *                         \p string
 *
 * @return                 the command_line_token that represents \p string
 *
 * \p string does not need to be a valid token
 */
enum command_line_token
str2command_line_token(struct filters_context *ctx, const char *string,
                       int *pe_index);

typedef void (*parse_callback)(struct filters_context *ctx,
                               int *arg_idx, const struct rbh_filter *filter,
                               struct rbh_filter_sort **sorts,
                               size_t *sort_count,
                               enum command_line_token token, void *param);

/**
 * parse_expression - parse an expression (predicates / operators / actions)
 *
 * @param ctx           filters context
 * @param arg_idx       a pointer to the index of argv to start parsing at
 * @param _filter       a filter (the part of the cli already parsed)
 * @param sorts         an array of filtering options
 * @param sorts_count   the size of \p sorts
 * @param cb            parsing callback to parse specific token
 * @param cb_param      argument for the parsing callback
 *
 * @return              a filter that represents the parsed expression
 *
 * Note this function is recursive
 */
struct rbh_filter *
parse_expression(struct filters_context *ctx, int *arg_idx,
                 const struct rbh_filter *_filter,
                 struct rbh_filter_sort **sorts, size_t *sorts_count,
                 parse_callback cb, void *cb_param);

#endif
