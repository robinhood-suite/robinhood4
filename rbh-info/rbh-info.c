/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <stdbool.h>
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
#include <robinhood/utils.h>
#include <robinhood/config.h>

#include "info.h"

static int
help()
{
    const char *message =
        "Usage: %s [-hl]\n"
        "\n"
        "Show information about plugins\n"
        "\n"
        "Optional arguments:\n"
        "    -h, --help             Show this message and exit\n"
        "    -l, --list             Show the list of installed plugins\n"
        "\n"
        "Usage: %s [-h] PLUGIN\n"
        "\n"
        "Show capabilities of the given plugin\n"
        "\n"
        "Positional argument:\n"
        "    PLUGIN                 a robinhood plugin\n"
        "\n"
        "Optional arguments:\n"
        "    -h, --help             Show this message and exit\n"
        "\n"
        "Plugins capabilities list:\n"
        "- filter: The ability to read the data after filtering it according to"
        " different criteria\n"
        "- synchronisation: The ability to read the data\n"
        "- update: The ability to update information or metadata of files in"
        " the backend\n"
        "- branch: The ability to read data over a subsection of a backend\n"
        "\n"
        "Usage: %s [PRE_URI_OPTIONS] URI [POST_URI_OPTIONS]\n"
        "\n"
        "Show information about the given URI\n"
        "\n"
        "Positional arguments:\n"
        "    URI                    a robinhood URI\n"
        "\n"
        "Pre URI optional arguments:\n"
        "    -c, --config          The configuration file to use\n"
        "    -h, --help            Show this message and exit\n"
        "\n"
        "Post URI optional arguments:\n"
        "    -a, --avg-obj-size     Show the average size of objects inside\n"
        "                           a given backend\n"
        "    -b, --backend-source   Show the backend used as source for past\n"
        "                           rbh-syncs\n"
        "    -c, --count            Show the amount of document inside a\n"
        "                           given backend\n"
        "    -f, --first-sync       Show infos about the first rbh-sync done\n"
        "    -y, --last-sync        Show infos about the last rbh-sync done\n"
        "    -m, --mountpoint       Show the mountpoint used as source for\n"
        "                           the last rbh-sync\n"
        "    -s, --size             Show the size of entries collection\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n\n";
            return printf(message, program_invocation_short_name,
                          program_invocation_short_name,
                          program_invocation_short_name);
}

static void
apply_command_options(int argc, char *argv[])
{
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            help();
            exit(0);
        }

        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc)
                error(EXIT_FAILURE, EINVAL,
                      "missing configuration file value");

            rbh_config_load_from_path(argv[i + 1]);
        }

        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            list_plugins_and_extensions();
            exit(0);
        }
    }
}

int
main(int _argc, char **_argv)
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "avg-obj-size",
            .val = 'a',
        },
        {
            .name = "backend-source",
            .val = 'b',
        },
        {
            .name = "count",
            .val = 'c',
        },
        {
            .name = "first-sync",
            .val = 'f',
        },
        {
            .name = "help",
            .val = 'h'
        },
        {
            .name = "last-sync",
            .val = 'y',
        },
        {
            .name = "list",
            .val = 'l'
        },
        {
            .name = "mountpoint",
            .val = 'm'
        },
        {
            .name = "size",
            .val = 's',
        },
        {}
    };
    const struct rbh_backend_plugin *plugin;
    struct rbh_backend *from = NULL;
    int flags = 0, option;
    const char *name;
    int nb_cli_args;
    char **argv;
    int rc = 0;
    int argc;

    nb_cli_args = rbh_count_args_before_uri(_argc, _argv);
    apply_command_options(nb_cli_args, _argv);

    if (nb_cli_args == _argc) {
        argc = _argc - 1;
        argv = &_argv[1];
    } else {
        argc = _argc - nb_cli_args;
        argv = &_argv[nb_cli_args];
    }

    while ((option = getopt_long(argc, argv, "abcfhlmsy", LONG_OPTIONS,
                                 NULL)) != -1) {
        switch (option) {
        case 'a':
            flags |= RBH_INFO_AVG_OBJ_SIZE;
            break;
        case 'b':
            flags |= RBH_INFO_BACKEND_SOURCE;
            break;
        case 'c':
            flags |= RBH_INFO_COUNT;
            break;
        case 'f':
            flags |= RBH_INFO_FIRST_SYNC;
            break;
        case 'h':
            help();
            return 0;
        case 'l':
            list_plugins_and_extensions();
            return 0;
        case 'm':
            flags |= RBH_INFO_MOUNTPOINT;
            break;
        case 's':
            flags |= RBH_INFO_SIZE;
            break;
        case 'y':
            flags |= RBH_INFO_LAST_SYNC;
            break;
        default :
            fprintf(stderr, "Unrecognized option\n");
            help();
            return EINVAL;
        }
    }

    if (argc < 1)
        error(EX_USAGE, 0, "not enough arguments\n");

    if (!rbh_is_uri(argv[0])) {
        // Print capabilities of the given plugin
        rc = capabilities_translate(argv[0]);
        goto end;
    }

    from = rbh_backend_from_uri(argv[0], true);
    name = from->name;
    plugin = rbh_backend_plugin_import(name);

    if (plugin == NULL) {
        fprintf(stderr, "This plugin does not exist\n");
        rc = EINVAL;
        goto end;
    }

    if (flags)
        rc = print_info_fields(from, flags);
    else
        info_translate(plugin);

end:
    if (from) {
        rbh_backend_destroy(from);
        rbh_backend_plugin_destroy(name);
    }

    return rc;
}
