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

static void
parse_expression(int arg_idx)
{
    for (int i = arg_idx; i < argc; i++) {
        enum command_line_token token;

        token = str2command_line_token(argv[i]);
        switch (token) {
        default:
            error(EXIT_FAILURE, ENOSYS, argv[i]);
        }
    }
}

static void
destroy_backends(void)
{
    for (size_t i = 0; i < uri_count; i++)
        rbh_backend_destroy(backends[i]);
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

    backends = malloc(uri_count * sizeof(*backends));
    if (backends == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    for (size_t i = 0; i < uri_count; i++)
        backends[i] = rbh_backend_from_uri(argv[i]);

    _atexit(destroy_backends);

    parse_expression(uri_count);
    find(ACT_PRINT, NULL);

    return EXIT_SUCCESS;
}
