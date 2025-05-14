/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#include <dlfcn.h>
#include <stdlib.h>
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

static int
undelete_helper()
{
    const char *message =
        "Usage:"
        "  %s -arguments General informations about rbh-undelete command\n"
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

    return EXIT_SUCCESS;
}
