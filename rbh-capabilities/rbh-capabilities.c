/* This file is part of Robinhood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#include "rbh-find/rbh-capabilities.h"

static struct rbh-backend*
load_backend_capabilities(char *uri)
{
    const struct rbh_backend_plugin *plugin;
    struct rbh_backend *backend;

    plugin = rbh_backend_plugin_import(uri); // Import backend plugin using URI

    if (plugin != NULL) {
        backend = rbh_backend_plugin_new(plugin, "none", NULL); // create a backend from a backend plugin


    } else {
        backend = rbh_backend_from_uri(uri);
        if (backend == NULL)
            error(EXIT_FAILURE, errno, "Unable to load backend %s", uri); // print error
    }

    return backend;
}

static int
capabilities_translate(struct rbh_backend *backend)
{
    printf("### The possible capabilities are : filter / update / branch ###\n");
    printf("Capabilities of %s :\n\n",backend->name);
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
    return 0
}


static int
usages()
{
    const char *message =
        "usage: %s BACKEND URI\n"
        "print backend URI or NAME capabilities\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n"
        "Where:\n"
        "    BACKEND  is the name of a backend\n"
        "    FSNAME   is the name of a filesystem for BACKEND\n"
        "    PATH/ID  is the path/id of an fsentry managed by BACKEND:FSNAME\n"
        "             (ID must be enclosed in square brackets '[ID]' to distinguish it\n"
        "             from a path)\n"
    return printf(message);
}



int
main(int argc, char **argv)
{
    if (argc == 2 && argv[2] != usages){
        struct rbh_backend *backend = load_backend_capabilities(argv[1]);
        capabilities_translate(backend);
    } else if (argc == 2 && argv[2] == usages){
        usages();
    } else {
        printf("Not enough arguments or no arguments");
        usages();
    }

    return 0;
}




