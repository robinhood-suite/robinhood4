/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

static void
usage(void)
{
    const char *message =
        "usage: %s [-h] [--raw] [--enrich MOUNTPOINT] SOURCE DESTINATION\n"
        "\n"
        "Collect changelog records from SOURCE, optionally enrich them with data\n"
        "collected from MOUNTPOINT and send them to DESTINATION.\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE          can be one of:\n"
        "                        a path to a yaml file, or '-' for stdin;\n"
        "                        an MDT name (eg. lustre-MDT0000).\n"
        "    DESTINATION     can be one of:\n"
        "                        '-' for stdout;\n"
        "                        a RobinHood URI (eg. rbh:mongo:test).\n"
        "\n"
        "Optional arguments:\n"
        "    -h, --help      print this message and exit\n"
        "    -r, --raw       do not enrich changelog records (default)\n"
        "    -e, --enrich MOUNTPOINT\n"
        "                    enrich changelog records by querying MOUNTPOINT as needed\n"
        "\n"
        "Note that uploading raw records to a RobinHood backend will fail, they have to\n"
        "be enriched first.\n";

    printf(message, program_invocation_short_name);
}

static const char *mountpoint;

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "enrich",
            .has_arg = required_argument,
            .val = 'e',
        },
        {
            .name = "help",
            .val = 'h',
        },
        {
            .name = "raw",
            .val = 'r',
        },
        {}
    };
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "e:hr", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'e':
            mountpoint = optarg;
            break;
        case 'h':
            usage();
            return 0;
        case 'r':
            mountpoint = NULL;
            break;
        case '?':
        default:
            /* getopt_long() prints meaningful error messages itself */
            exit(EX_USAGE);
        }
    }

    if (argc - optind < 2)
        error(EX_USAGE, 0, "not enough arguments");
    if (argc - optind > 2)
        error(EX_USAGE, 0, "too many arguments");

    /* TODO: parse SOURCE into an iterator yielding fsevents
     *       (potentially enriching them as needed/possible)
     */

    /* TODO: parse DESTINATION and feed it SOURCE's fsevents */

    error(EXIT_FAILURE, ENOSYS, "%s", __func__);
}
