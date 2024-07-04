/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
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
#include <sysexits.h>
#include <getopt.h>

#include <robinhood.h>
#include <robinhood/utils.h>
#include <robinhood/config.h>

static int
capabilities_translate(const struct rbh_backend_plugin *plugin)
{

    const uint8_t capabilities = plugin->capabilities;

    printf("Capabilities of %s:\n",plugin->plugin.name);
    if (capabilities & RBH_FILTER_OPS)
        printf("- filter\n");

    if (capabilities & RBH_UPDATE_OPS)
        printf("- update\n");

    if (capabilities & RBH_BRANCH_OPS)
        printf("- branch\n");

    return 0;
}

static int
help()
{
    const char *message =
        "Usage:"
        "  %s <name of backend>   Show capabilities of the given backend"
        " name\n"
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
    const struct rbh_backend_plugin *plugin;
    const char *arg;
    int option;

    if (argc == 1){
        printf("No backend name given, Please give a backend name\n");
        help();
        return EINVAL;
    }

    while ((option = getopt_long(argc, argv, "h", LONG_OPTIONS,
                                 NULL)) != -1) {
        switch (option) {
        case 'h':
            help();
            return 0;
        default :
            fprintf(stderr, "Unrecognized option\n\n");
            help();
            return EINVAL;
        }
    }

    arg = argv[optind];
    plugin = rbh_backend_plugin_import(arg);

    if (plugin == NULL) {
        fprintf(stderr, "This backend does not exist\n");
        return EINVAL;
    } else {
        capabilities_translate(plugin);
        return 0;
    }
}
