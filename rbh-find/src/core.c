/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <sysexits.h>

#include "rbh-find/core.h"

int
rbh_find_count_args_before_uri(int argc, char **argv)
{
    size_t count = 0;

    while (count < argc && !rbh_is_uri(argv[count]))
        count++;

    return count;
}

void
ctx_finish(struct find_context *ctx)
{
    const char *name;

    for (size_t i = 0; i < ctx->backend_count; i++) {
        name = ctx->backends[i]->name;
        rbh_backend_destroy(ctx->backends[i]);
        rbh_backend_plugin_destroy(name);
    }
    free(ctx->backends);
    free(ctx->info_pe);
}

enum command_line_token
str2command_line_token(struct find_context *ctx, const char *string,
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

static int complete_rbh_filter(const struct rbh_filter *filter,
                               struct rbh_backend *backend,
                               const struct rbh_filter_options *options,
                               const struct rbh_filter_output *output);

static int
complete_logical_filter(const struct rbh_filter *filter,
                        struct rbh_backend *backend,
                        const struct rbh_filter_options *options,
                        const struct rbh_filter_output *output)
{
    for (uint32_t i = 0; i < filter->logical.count; i++) {
        if (complete_rbh_filter(filter->logical.filters[i], backend, options,
                                output))
            return -1;
    }

    return 0;
}

static void
update_statx_rbh_value(struct rbh_filter *filter,
                       const struct rbh_filter_field *field,
                       const struct rbh_statx *statx)
{
    switch (field->statx) {
    case RBH_STATX_MTIME_SEC:
        filter->compare.value.uint64 = statx->stx_mtime.tv_sec;
        return;
    default:
        return;
    }
}

static void
update_rbh_value(struct rbh_filter *filter,
                 const struct rbh_filter_field *field,
                 struct rbh_fsentry *fsentry)
{
    switch (field->fsentry) {
    case RBH_FP_STATX:
        return update_statx_rbh_value(filter, field, fsentry->statx);
    default:
        return;
    }
}

static int
complete_get_filter(const struct rbh_filter *filter,
                    struct rbh_backend *backend,
                    const struct rbh_filter_options *options,
                    const struct rbh_filter_output *output)
{
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *fsentry;

    fsentries = rbh_backend_filter(backend, filter->get.fsentry_to_get, options,
                                   output);
    if (fsentries == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "filter_fsentries");

    fsentry = rbh_mut_iter_next(fsentries);
    if (fsentry == NULL) {
        rbh_mut_iter_destroy(fsentries);
        return -1;
    }

    update_rbh_value(filter->get.filter, &filter->get.field, fsentry);

    free(fsentry);
    rbh_mut_iter_destroy(fsentries);

    return 0;
}

static int
complete_rbh_filter(const struct rbh_filter *filter,
                    struct rbh_backend *backend,
                    const struct rbh_filter_options *options,
                    const struct rbh_filter_output *output)
{
    if (rbh_is_logical_operator(filter->op))
        return complete_logical_filter(filter, backend, options, output);

    if (rbh_is_get_operator(filter->op))
        return complete_get_filter(filter, backend, options, output);

    return 0;
}

size_t
_find(struct find_context *ctx, int backend_index, enum action action,
      const struct rbh_filter *filter, const struct rbh_filter_sort *sorts,
      size_t sorts_count)
{
    const struct rbh_filter_options OPTIONS = {
        .sort = {
            .items = sorts,
            .count = sorts_count
        },
    };
    const struct rbh_filter_output OUTPUT = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = RBH_FP_ALL,
            .statx_mask = RBH_STATX_ALL,
        },
    };
    struct rbh_mut_iterator *fsentries;
    size_t count = 0;

    /**
     * Some filters need to be completed with value from the backend before
     * doing the find.
     */
    if (ctx->need_prefetch &&
        complete_rbh_filter(filter, ctx->backends[backend_index], &OPTIONS,
                            &OUTPUT))
        return count;

    fsentries = rbh_backend_filter(ctx->backends[backend_index], filter,
                                   &OPTIONS, &OUTPUT);
    if (fsentries == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "filter_fsentries");

    do {
        struct rbh_fsentry *fsentry;

        do {
            errno = 0;
            fsentry = rbh_mut_iter_next(fsentries);
        } while (fsentry == NULL && errno == EAGAIN);

        if (fsentry == NULL)
            break;

        count += ctx->exec_action_callback(ctx, backend_index, action, fsentry);
        free(fsentry);
    } while (true);

    if (errno != ENODATA)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_mut_iter_next");

    rbh_mut_iter_destroy(fsentries);

    return count;
}

void
find(struct find_context *ctx, enum action action, int *arg_idx,
     const struct rbh_filter *filter, const struct rbh_filter_sort *sorts,
     size_t sorts_count)
{
    int i = *arg_idx;
    size_t count = 0;

    ctx->action_done = true;

    i += ctx->pre_action_callback(ctx, i, action);

    for (size_t i = 0; i < ctx->backend_count; i++)
        count += _find(ctx, i, action, filter, sorts, sorts_count);

    ctx->post_action_callback(ctx, i, action, count);

    *arg_idx = i;
}

struct rbh_filter *
parse_expression(struct find_context *ctx, int *arg_idx,
                 const struct rbh_filter *_filter,
                 struct rbh_filter_sort **sorts, size_t *sorts_count)
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
        bool ascending = true;
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
            tmp = parse_expression(ctx, &i, &negated_left_filter, sorts,
                                   sorts_count);
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
            tmp = parse_expression(ctx, &i, &left_filter, sorts, sorts_count);
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
        case CLT_RSORT:
            /* Set a descending sort option */
            ascending = false;
            __attribute__((fallthrough));
        case CLT_SORT:
            /* Build an options filter from the sort command and its arguments */
            if (i + 1 >= ctx->argc)
                error(EX_USAGE, 0, "missing argument to '%s'", ctx->argv[i]);
            *sorts = sort_options_append(*sorts, (*sorts_count)++,
                                         str2field(ctx->argv[++i]), ascending);
            break;
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
        case CLT_ACTION:
            ctx->action_done = true;
            find(ctx, str2action(ctx->argv[i]), &i, &left_filter, *sorts,
                 *sorts_count);
            break;
        }
    }

    *arg_idx = i;
    return filter;
}
