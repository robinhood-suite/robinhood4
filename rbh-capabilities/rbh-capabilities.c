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

static const struct rbh_backend_plugin *
import_backend_plugin(const char *name)
{
    const struct rbh_backend_plugin *plugin;
    plugin = rbh_backend_plugin_import(name);

    if (plugin != NULL){
        return plugin;
    } else {
        printf("This backend does not exist\n\n");
        return NULL;
    };
}

static int
capabilities_translate(const struct rbh_backend_plugin *plugin)
{

    const uint8_t capabilities = plugin->capabilities;
    printf("Capabilities of %s :\n\n",plugin->plugin.name);
    if (capabilities & RBH_FILTER_OPS)
    {
        printf("- filter\n");
    }
    if (capabilities & RBH_UPDATE_OPS)
    {
        printf("- update\n");
    }
    if (capabilities & RBH_BRANCH_OPS)
    {
        printf("- branch\n");
    }
    printf("\n");
    return 0;
}

static int
help()
{
    const char *message =
        "Usage:\n"
        "  %s <name of backend>   Show capabilities of the given backend"
        "name\n"
        "  --help                 Show this message and exit\n"
        "  --list                 Show the list of backends\n\n";
    return printf(message, program_invocation_short_name);
}

static int
rbh_backend_list()
{
    const char *message =
        "List of backends in robinhood:\n"
        "- hestia\n"
        "- lustre\n"
        "- lustre_mpi\n"
        "- mongo\n"
        "- posix\n"
        "- posix_mpi\n\n";
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
        {
            .name = "list",
            .val = 'l'
        },
        {}
    };
    int option;

    if (argc == 1){
        printf("No backend name given, Please give a backend name\n");
        help();
        return EINVAL;
    }

    while ((option = getopt_long(argc, argv, "hl", LONG_OPTIONS,
                                 NULL)) != -1) {
        switch (option) {
        case 'h':
            help();
            return 0;
        case 'l':
            rbh_backend_list();
            return 0;
        default :
            fprintf(stderr, "Unrecognized option\n\n");
            help();
            return EINVAL;
        }
    }

    const char *arg = argv[optind];
    const struct rbh_backend_plugin *plugin = import_backend_plugin(arg);
    if (plugin == NULL) {
        return EINVAL;
    } else {
        capabilities_translate(plugin);
        return 0;
    }
}
