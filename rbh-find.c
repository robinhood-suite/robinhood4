/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "filters.h"
#include "parser.h"

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>

#include <sys/stat.h>

#include <robinhood.h>
#ifndef HAVE_STATX
# include <robinhood/statx.h>
#endif
#include <robinhood/utils.h>

static size_t uri_count = 0;
static struct rbh_backend **backends;

static size_t
_find(struct rbh_backend *backend, enum action action,
      const struct rbh_filter *filter)
{
    struct rbh_mut_iterator *fsentries;
    size_t count = 0;

    fsentries = rbh_backend_filter_fsentries(backend, filter, RBH_FP_ALL,
                                             STATX_ALL);
    if (fsentries == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "filter_fsentries");

    do {
        struct rbh_fsentry *fsentry;

        do {
            errno = 0;
            fsentry = rbh_mut_iter_next(fsentries);
        } while (fsentry == NULL && errno == EAGAIN);

        if (fsentry == NULL && errno == ENODATA)
            break;
        if (fsentry == NULL)
            break;

        switch (action) {
        case ACT_PRINT:
            /* TODO: support extended attributes in the RobinHood library
             *       and switch to printing paths
             */
            printf("%s\n", fsentry->name);
            break;
        case ACT_COUNT:
            count++;
            break;
        default:
            error(EXIT_FAILURE, ENOSYS, action2str(action));
            break;
        }
        free(fsentry);
    } while (true);

    if (errno != ENODATA)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_mut_iter_next");

    rbh_mut_iter_destroy(fsentries);

    return count;
}

static int argc;
static char **argv;
static bool did_something = false;

static void
find(enum action action, const struct rbh_filter *filter)
{
    size_t count = 0;

    did_something = true;

    for (size_t i = 0; i < uri_count; i++)
        count += _find(backends[i], action, filter);

    switch (action) {
    case ACT_COUNT:
        printf("%lu matching entries\n", count);
        break;
    default:
        break;
    }
}

static struct rbh_filter *
parse_predicate(int *arg_idx)
{
    enum predicate predicate;
    struct rbh_filter *filter;
    int i = *arg_idx;

    predicate = str2predicate(argv[i]);

    if (i + 1 >= argc)
        error(EX_USAGE, 0, "missing argument to `%s'", argv[i]);

    /* In the following block, functions should call error() themselves rather
     * than returning.
     *
     * Errors are most likely fatal (not recoverable), and this allows for
     * precise and meaningul error messages.
     */
    switch (predicate) {
    case PRED_AMIN:
    case PRED_MMIN:
    case PRED_CMIN:
        filter = xmin2filter(predicate, argv[++i]);
        break;
    case PRED_ATIME:
    case PRED_MTIME:
    case PRED_CTIME:
        filter = xtime2filter(predicate, argv[++i]);
        break;
    case PRED_NAME:
        filter = shell_regex2filter(predicate, argv[++i], 0);
        break;
    case PRED_INAME:
        filter = shell_regex2filter(predicate, argv[++i],
                                    RBH_FRO_CASE_INSENSITIVE);
        break;
    case PRED_TYPE:
        filter = filetype2filter(argv[++i]);
        break;
    default:
        error(EXIT_FAILURE, ENOSYS, argv[i]);
    }
    assert(filter != NULL);

    *arg_idx = i;
    return filter;
}

/**
 * parse_expression - parse a find expression (predicates / operators / actions)
 *
 * @param arg_idx   a pointer to the index of argv to start parsing at
 * @param _filter   a filter (the part of the cli parsed by the caller)
 *
 * @return          a filter that represents the parsed expression
 *
 * Note this function is recursive and will call find() itself if it parses an
 * action
 */
static struct rbh_filter *
parse_expression(int *arg_idx, const struct rbh_filter *_filter)
{
    enum command_line_token previous_token = CLT_URI;
    struct rbh_filter *filter = NULL;
    bool negate = false;
    int i;

    for (i = *arg_idx; i < argc; i++) {
        const struct rbh_filter *left_filters[2] = {filter, _filter};
        const struct rbh_filter left_filter = {
            .op = RBH_FOP_AND,
            .logical = {
                .count = 2,
                .filters = left_filters,
            },
        };
        const struct rbh_filter *ptr_to_left_filter = &left_filter;
        struct rbh_filter negated_left_filter = {
            .op = RBH_FOP_NOT,
            .logical = {
                .count = 1,
                .filters = &ptr_to_left_filter,
            },
        };
        const struct rbh_filter *filters[2] = {filter, NULL};
        enum command_line_token token;

        token = str2command_line_token(argv[i]);
        switch (token) {
        case CLT_URI:
            error(EX_USAGE, 0, "paths must preceed expression: %s", argv[i]);
        case CLT_AND:
        case CLT_OR:
            switch (previous_token) {
            case CLT_PREDICATE:
            case CLT_PARENTHESIS_CLOSE:
                break;
            default:
                error(EX_USAGE, 0,
                      "invalid expression; you have used a binary operator '%s' with nothing before it.",
                      argv[i]);
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
            filters[1] = parse_expression(&i, &negated_left_filter);
            /* parse_expression() returned, so it must have seen a closing
             * parenthesis or reached the end of the command line, we should
             * return here too.
             */

            /* "OR" the part of the left filter we parsed ourselves (ie. not
             * `_filter') and the right filter.
            */
            errno = 0;
            filter = rbh_filter_or_new(filters, 2);
            if (filter == NULL && errno != 0)
                error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__ - 2,
                              "filter_or");

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
            filters[1] = parse_expression(&i, NULL);
            if (i >= argc
                    || str2command_line_token(argv[i]) != CLT_PARENTHESIS_CLOSE)
                error(EX_USAGE, 0,
                      "invalid expression; I was expecting to find a ')' somewhere but did not see one.");

            /* Negate the sub-expression's filter, if need be */
            if (negate) {
                errno = 0;
                filters[1] = rbh_filter_not_new(filters[1]);
                if (filters[1] == NULL && errno != 0)
                    error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__ - 2,
                                  "filter_not");
                negate = false;
            }

            /* Build the resulting filter and continue */
            errno = 0;
            filter = rbh_filter_and_new(filters, 2);
            if (filter == NULL && errno != 0)
                error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__ - 2,
                              "filter_and");
            break;
        case CLT_PARENTHESIS_CLOSE:
            if (previous_token == CLT_PARENTHESIS_OPEN)
                error(EXIT_FAILURE, 0, "invalid expression; empty parentheses are not allowed.");
            /* End of a sub-expression, update arg_idx and return */
            *arg_idx = i;
            return filter;
        case CLT_PREDICATE:
            /* Build a filter from the predicate and its arguments */
            filters[1] = parse_predicate(&i);
            if (negate) {
                errno = 0;
                filters[1] = rbh_filter_not_new(filters[1]);
                if (filters[1] == NULL && errno != 0)
                    error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__ - 2,
                                  "filter_not");
                negate = false;
            }

            errno = 0;
            filter = rbh_filter_and_new(filters, 2);
            if (filter == NULL && errno != 0)
                error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__ - 2,
                              "filter_and");
            break;
        case CLT_ACTION:
            find(str2action(argv[i]), &left_filter);
            break;
        }
        previous_token = token;
    }

    *arg_idx = i;
    return filter;
}

static void
destroy_backends(void)
{
    for (size_t i = 0; i < uri_count; i++)
        rbh_backend_destroy(backends[i]);
    free(backends);
}

static void
_atexit(void (*function)(void))
{
    if (atexit(function)) {
        function();
        error(EXIT_FAILURE, 0, "atexit: too many functions registered");
    }
}

int
main(int _argc, char *_argv[])
{
    struct rbh_filter *filter;

    /* Discard the program's name */
    argc = _argc - 1;
    argv = &_argv[1];

    /* Parse the command line */
    for (uri_count = 0; uri_count < argc; uri_count++) {
        if (str2command_line_token(argv[uri_count]) != CLT_URI)
            break;
    }

    if (uri_count == 0)
        error(EX_USAGE, 0, "missing at least one robinhood URI");

    _atexit(destroy_backends);
    backends = malloc(uri_count * sizeof(*backends));
    if (backends == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    for (size_t i = 0; i < uri_count; i++)
        backends[i] = rbh_backend_from_uri(argv[i]);

    _argc = uri_count;
    filter = parse_expression(&_argc, NULL);
    if (_argc != argc)
        error(EX_USAGE, 0, "you have too many ')'");

    if (!did_something)
        find(ACT_PRINT, filter);

    rbh_filter_free(filter);

    return EXIT_SUCCESS;
}
