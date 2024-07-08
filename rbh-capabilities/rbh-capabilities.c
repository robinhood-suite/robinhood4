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
load_backend_capabilities(const char *name)
{
    const struct rbh_backend_plugin *plugin;

    if (rbh_backend_plugin_import(name) != NULL){
        plugin = rbh_backend_plugin_import(name);
        return plugin;
    } else {
        printf("This backend does not exist\n");
        return NULL;
    };
}

static int
capabilities_translate(const struct rbh_backend_plugin *plugin)
{
    if (plugin == NULL){
        printf("Plugin is NULL in capabilities_translate\n");
        return -1;
    }

    const uint8_t capabilities = plugin->capabilities;
    printf("Capabilities of %s :\n\n",plugin->plugin.name);
    if (capabilities & 0b100)
    {
        printf("- filter\n");
    }
    if (capabilities & 0b010)
    {
        printf("- update\n");
    }
    if (capabilities & 0b001)
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
        "\nUsage:\n"
        "  %s <name of backend>   Show capabilities with of the given backend"
        "name\n"
        "  --help                 Show usages information\n";
    return printf(message,program_invocation_short_name);
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

    if (argc == 1){
        printf("Not enough arguments or no argument\n");
        help();
        return 0;
    }

    while ((option = getopt_long(argc, argv, "h", LONG_OPTIONS,
                                 NULL)) != -1) {
        switch (option) {
        case 'h':
            help();
            return 0;
        default :
            printf("Unrecognized option\n");
            help();
            return 0;

        }
    }

    if (optind < argc){
        const char *arg = argv[optind];
        printf("Argument provided: %s\n", arg);
        const struct rbh_backend_plugin *plugin = load_backend_capabilities(arg);
        if (plugin != NULL) {
            capabilities_translate(plugin);
        }
    } else {
        printf("Not enough arguments or no argument \n");
        help();
        return 0;
    };

    return 0;
}
