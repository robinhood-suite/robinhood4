/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#include <dlfcn.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sysexits.h>
#include <getopt.h>

#include <robinhood.h>
#include <robinhood/config.h>

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

static int
undelete_helper()
{
    const char *message =
        "Usage:"
        "  %s <URI> -arguments       General informations about rbh-undelete"
        " command\n"
        "  -h --help                 Show this message and exit\n";
    return printf(message, program_invocation_short_name);
}

int
main(int argc, char **argv)
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "help",
            .val = 'h'
        },
        {}
    };
    int option;

    while ((option = getopt_long(argc, argv, "h", LONG_OPTIONS,
                                 NULL)) != -1) {
        switch (option) {
        case 'h':
            undelete_helper();
            return 0;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1)
        error(EX_USAGE, 0, "not enough arguments");
    if (argc > 2)
        error(EX_USAGE, 0, "too much arguments");

    from = rbh_backend_from_uri(argv[0], true);

    printf("backend name: %s\n", from->name);

    return EXIT_SUCCESS;
}
