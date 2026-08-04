/* This file is part of RobinHood
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_PARSER_H
#define ROBINHOOD_PARSER_H

#include "robinhood/filters/core.h"

enum command_line_token {
    CLT_URI,
    CLT_AND,
    CLT_OR,
    CLT_NOT,
    CLT_PARENTHESIS_OPEN,
    CLT_PARENTHESIS_CLOSE,
    CLT_PREDICATE,
    CLT_ACTION,
};

enum output_modifier {
    OUTPUT_MODIFIER_NONE,
    OUTPUT_MODIFIER_SORT,
    OUTPUT_MODIFIER_RSORT,
    OUTPUT_MODIFIER_LIMIT,
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

/**
 * str2output_modifier - output modifier classifier
 *
 * @param string           the string to classify
 *
 * @return                 the output_modifier that represents \p string
 */
enum output_modifier
str2output_modifier(const char *string);

typedef void (*parse_clt_callback)(struct filters_context *ctx,
                                   int *arg_idx, const struct rbh_filter *filter,
                                   struct rbh_filter_options *options,
                                   enum command_line_token token, void *param);

typedef void (*parse_om_callback)(struct filters_context *ctx, int *arg_idx,
                                  struct rbh_filter_options *options,
                                  enum output_modifier modifier, void *param);

/**
 * parse_expression - parse an expression (predicates / operators / actions)
 *
 * @param ctx           filters context
 * @param arg_idx       a pointer to the index of argv to start parsing at
 * @param _filter       a filter (the part of the cli already parsed)
 * @param options       filtering options and modifiers
 * @param clt_cb        parsing callback to parse a specific command line token
 * @param om_cb         parsing callback to parse a sepcific output modifier
 * @param cb_param      argument for the parsing callback
 *
 * @return              a filter that represents the parsed expression
 *
 * Note this function is recursive
 */
struct rbh_filter *
parse_expression(struct filters_context *ctx, int *arg_idx,
                 const struct rbh_filter *_filter,
                 struct rbh_filter_options *options,
                 parse_clt_callback clt_cb,
                 parse_om_callback om_cb,
                 void *clt_cb_param, void *om_cb_param);

#endif
