/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "robinhood/filters/parser.h"

enum command_line_token
str2command_line_token(struct filters_context *ctx, const char *string,
                       int *pe_index)
{
    switch (string[0]) {
    case '(':
        if (string[1] == '\0')
            return CLT_PARENTHESIS_OPEN;
        break;
    case ')':
        if (string[1] == '\0')
            return CLT_PARENTHESIS_CLOSE;
        break;
    case '!':
        if (string[1] == '\0')
            return CLT_NOT;
        break;
    case '-':
        switch (string[1]) {
        case 'a':
            if (string[2] == '\0' || strcmp(&string[2], "nd") == 0)
                return CLT_AND;
            break;
        case 'o':
            if (string[2] == '\0' || strcmp(&string[2], "r") == 0)
                return CLT_OR;
            break;
        case 'n':
            if (strcmp(&string[2], "ot") == 0)
                return CLT_NOT;
            break;
        case 'r':
            if (strcmp(&string[2], "sort") == 0)
                return CLT_RSORT;
            break;
        case 's':
            if (strcmp(&string[2], "ort") == 0)
                return CLT_SORT;
            break;
        }

        for (int i = 0; i < ctx->info_pe_count; ++i) {
            const struct rbh_pe_common_operations *common_ops =
                get_common_operations(&ctx->info_pe[i]);
            enum rbh_parser_token token =
                rbh_pe_common_ops_check_valid_token(common_ops, string);

            if (token == RBH_TOKEN_PREDICATE) {
                *pe_index = i;
                return CLT_PREDICATE;
            }
        }

        return CLT_ACTION;
    }
    return CLT_URI;
}
