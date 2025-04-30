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

#endif
