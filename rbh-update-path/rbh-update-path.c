/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <error.h>
#include <getopt.h>
#include <sysexits.h>

#include <robinhood.h>

static struct rbh_backend *backend;

static void __attribute__((destructor))
destroy_backend(void)
{
    const char *name;

    if (backend) {
        name = backend->name;
        rbh_backend_destroy(backend);
        rbh_backend_plugin_destroy(name);
    }
}

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "config",
            .has_arg = required_argument,
            .val = 'c',
        },
        {}
    };
    int rc;
    char c;

    rc = rbh_config_from_args(argc - 1, argv + 1);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to open configuration file");

    while ((c = getopt_long(argc, argv, "c:", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'c':
            /* already parsed */
            break;
        case '?':
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

    backend = rbh_backend_from_uri(argv[0], false);

    return EXIT_SUCCESS;
}
