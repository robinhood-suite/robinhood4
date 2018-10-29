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

static void
_find(struct rbh_backend *backend, enum action action,
      const struct rbh_filter *filter)
{
    struct rbh_mut_iterator *fsentries;

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
        default:
            assert(false);
            break;
        }
        free(fsentry);
    } while (true);

    if (errno != ENODATA)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_mut_iter_next");

    rbh_mut_iter_destroy(fsentries);
}

static int argc;
static char **argv;

static void
find(enum action action, const struct rbh_filter *filter)
{
    for (size_t i = 0; i < uri_count; i++)
        _find(backends[i], action, filter);
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
    case PRED_NAME:
        filter = shell_regex2filter(predicate, argv[++i], 0);
        break;
    default:
        error(EXIT_FAILURE, ENOSYS, argv[i]);
    }
    assert(filter != NULL);

    *arg_idx = i;
    return filter;
}

static struct rbh_filter *
parse_expression(int arg_idx)
{
    struct rbh_filter *filter = NULL;
    bool negate = false;

    for (int i = arg_idx; i < argc; i++) {
        const struct rbh_filter *filters[2] = {filter, NULL};
        enum command_line_token token;

        token = str2command_line_token(argv[i]);
        switch (token) {
        case CLT_URI:
            error(EX_USAGE, 0, "paths must preceed expression: %s", argv[i]);
        case CLT_NOT:
            negate = !negate;
            break;
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
        default:
            error(EXIT_FAILURE, ENOSYS, argv[i]);
        }
    }

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

    filter = parse_expression(uri_count);
    find(ACT_PRINT, filter);

    rbh_filter_free(filter);

    return EXIT_SUCCESS;
}
