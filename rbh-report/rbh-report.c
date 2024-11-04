/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <robinhood.h>
#include <robinhood/backend.h>
#include <robinhood/uri.h>

static struct rbh_backend *from;

static void __attribute__((destructor))
destroy_from(void)
{
    const char *name;

    if (from) {
        name = from->name;
        rbh_backend_destroy(from);
        rbh_backend_plugin_destroy(name);
    }
}

static void
report()
{
    struct rbh_filter_options options = {
        .skip = 0,
        .limit = 0,
        .skip_error = false,
        .sort = {
            .items = NULL,
            .count = 0,
        },
    };
    struct rbh_mut_iterator *iter;

    iter = rbh_backend_report(from, NULL, &options);

    if (iter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_backend_report");

    do {
        struct rbh_fsentry *fsentry = rbh_mut_iter_next(iter);

        if (fsentry == NULL)
            break;

        errno = ENOTSUP;
        return;
    } while (true);

    errno = ENOTSUP;
    return;
}

/*----------------------------------------------------------------------------*
 |                                    cli                                     |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                              usage()                               |
     *--------------------------------------------------------------------*/

static int
usage(void)
{
    const char *message =
        "usage: %s [-h] SOURCE\n"
        "\n"
        "Create a report from SOURCE's entries\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE  a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    -h,--help             show this message and exit\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n"
        "Where:\n"
        "    BACKEND  is the name of a backend\n"
        "    FSNAME   is the name of a filesystem for BACKEND\n"
        "    PATH/ID  is the path/id of an fsentry managed by BACKEND:FSNAME\n"
        "             (ID must be enclosed in square brackets '[ID]' to distinguish it\n"
        "             from a path)\n";

    return printf(message, program_invocation_short_name);
}

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "help",
            .val = 'h',
        },
        {}
    };
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "h", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'h':
            usage();
            return 0;
        default:
            /* getopt_long() prints meaningful error messages itself */
            exit(EX_USAGE);
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1)
        error(EX_USAGE, 0, "not enough arguments");
    if (argc > 1)
        error(EX_USAGE, 0, "unexpected argument: %s", argv[1]);

    /* Parse SOURCE */
    from = rbh_backend_from_uri(argv[0]);

    report();

    return EXIT_SUCCESS;
}
