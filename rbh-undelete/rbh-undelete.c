/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#include <sysexits.h>
#include <getopt.h>

#include <robinhood.h>
#include <robinhood/config.h>

static struct rbh_backend *metadata_source, *target_entry;

static void __attribute__((destructor))
destroy_metadata_source_and_target_entry(void)
{
    const char *name;

    if (metadata_source) {
        name = metadata_source->name;
        rbh_backend_destroy(metadata_source);
        rbh_backend_plugin_destroy(name);
    }

    if (target_entry) {
        name = target_entry->name;
        rbh_backend_destroy(target_entry);
        rbh_backend_plugin_destroy(name);
    }
}

/*----------------------------------------------------------------------------*
 |                                    cli                                     |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                              usage()                               |
     *--------------------------------------------------------------------*/

static int
usage()
{
    const char *message =
        "Usage: %s [-h|--help] SOURCE DEST\n"
        "\n"
        "Undelete DEST's entry using SOURCES's metadata\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE   a robinhood URI\n"
        "    DEST     a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    -h,--help            Show this message and exit\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n\n";

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
            usage();
            return 0;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1)
        error(EX_USAGE, 0, "not enough arguments");
    if (argc > 2)
        error(EX_USAGE, 0, "too many arguments");

    metadata_source = rbh_backend_from_uri(argv[0], true);
    target_entry = rbh_backend_from_uri(argv[1], true);

    return EXIT_SUCCESS;
}
