/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sysexits.h>

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

struct rbh_filter *
parse_expression(struct filters_context *ctx, int *arg_idx,
                 const struct rbh_filter *_filter,
                 struct rbh_filter_options *options,
                 parse_callback cb, void *cb_param)
{
    static enum command_line_token token = CLT_URI;
    struct rbh_filter *filter = NULL;
    bool negate = false;
    int i;

    for (i = *arg_idx; i < ctx->argc; i++) {
        const struct rbh_filter *left_filters[2] = {filter, _filter};
        const struct rbh_filter left_filter = {
            .op = RBH_FOP_AND,
            .logical = {
                .filters = left_filters,
                .count = 2,
            },
        };
        const struct rbh_filter *ptr_to_left_filter = &left_filter;
        struct rbh_filter negated_left_filter = {
            .op = RBH_FOP_NOT,
            .logical = {
                .filters = &ptr_to_left_filter,
                .count = 1,
            },
        };
        enum command_line_token previous_token = token;
        struct rbh_filter *tmp;
        int pe_index = -1;

        token = str2command_line_token(ctx, ctx->argv[i], &pe_index);
        switch (token) {
        case CLT_URI:
            error(EX_USAGE, 0, "paths must preceed expression: %s",
                  ctx->argv[i]);
            __builtin_unreachable();
        case CLT_AND:
        case CLT_OR:
            switch (previous_token) {
            case CLT_ACTION:
            case CLT_PREDICATE:
            case CLT_PARENTHESIS_CLOSE:
                break;
            default:
                error(EX_USAGE, 0,
                      "invalid expression; you have used a binary operator '%s' with nothing before it.",
                      ctx->argv[i]);
            }

            /* No further processing needed for CLT_AND */
            if (token == CLT_AND)
                break;

            /* The -o/-or operator is tricky to implement!
             *
             * It works this way: any entry that does not match the left
             * condition is checked against the right one. Readers should note
             * that an entry that matches the left condition _is not checked_
             * against the right condition.
             *
             * GNU-find can probably do this in a single filesystem scan, but we
             * cannot. We have to build a filter for the right condition that
             * excludes entries matched by the left condition.
             *
             * Basically, when we read: "<cond-A> -o <cond-B>", we
             * translate it to "<cond-A> -o (! <cond-A> -a <cond-B>)"
             *
             * An example might help:
             * -name -a -or -name -b <=> any entry whose name matches 'a' or
             *                           doesn't match 'a' but matches 'b'
             */

            /* Consume the -o/-or token */
            i++;

            /* Parse the filter at the right of -o/-or */
            tmp = parse_expression(ctx, &i, &negated_left_filter,
                                   options, cb, cb_param);
            /* parse_expression() returned, so it must have seen a closing
             * parenthesis or reached the end of the command line, we should
             * return here too.
             */

            /* "OR" the part of the left filter we parsed ourselves (ie. not
             * `_filter') and the right filter.
            */
            filter = rbh_filter_or(filter, tmp);

            /* Update arg_idx and return */
            *arg_idx = i;
            return filter;
        case CLT_NOT:
            negate = !negate;
            break;
        case CLT_PARENTHESIS_OPEN:
            /* Consume the ( token */
            i++;

            /* Parse the sub-expression */
            tmp = parse_expression(ctx, &i, &left_filter, options,
                                   cb, cb_param);
            if (i >= ctx->argc || token != CLT_PARENTHESIS_CLOSE)
                error(EX_USAGE, 0,
                      "invalid expression; I was expecting to find a ')' somewhere but did not see one.");

            /* Negate the sub-expression's filter, if need be */
            if (negate) {
                tmp = rbh_filter_not(tmp);
                negate = false;
            }

            /* Build the resulting filter and continue */
            if (filter == NULL)
                filter = tmp;
            else
                filter = rbh_filter_and(filter, tmp);
            break;
        case CLT_PARENTHESIS_CLOSE:
            if (previous_token == CLT_PARENTHESIS_OPEN)
                error(EXIT_FAILURE, 0,
                      "invalid expression; empty parentheses are not allowed.");
            /* End of a sub-expression, update arg_idx and return */
            *arg_idx = i;
            return filter;
        case CLT_PREDICATE:
            assert(pe_index != -1);
            /* Build a filter from the predicate and its arguments */
            {
                const struct rbh_pe_common_operations *common_ops =
                    get_common_operations(&ctx->info_pe[pe_index]);
                tmp = rbh_pe_common_ops_build_filter(common_ops,
                                                     (const char **) ctx->argv,
                                                     ctx->argc, &i,
                                                     &ctx->need_prefetch);
            }

            if (negate) {
                tmp = rbh_filter_not(tmp);
                negate = false;
            }

            if (filter == NULL)
                filter = tmp;
            else
                filter = rbh_filter_and(filter, tmp);
            break;
        default:
            if (cb)
                cb(ctx, &i, &left_filter, options, token, cb_param);
            break;
        }

    }

    *arg_idx = i;
    return filter;
}
