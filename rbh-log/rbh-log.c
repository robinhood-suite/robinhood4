/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <error.h>
#include <getopt.h>
#include <sysexits.h>

#include <robinhood.h>

#include "log.h"

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

static void
usage(void)
{
    const char *message =
        "Usage: %s [OPTIONS] SOURCE\n"
        "\n"
        "Print logs from SOURCE's metadata\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE                 a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "   -c, --config PATH       the configuration file to use.\n"
        "   -h, --help              show this message and exit\n"
        "   -l, --last N            print the last N logs\n"
        "    --version              print RobinHood 4's version\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n";

    printf(message, program_invocation_short_name);
}

static void
print_logs(const struct rbh_value_map *logs, const char *header)
{
    for (size_t i = 0 ; i < logs->count ; i++)
        print_sync_log(&logs->pairs[i].value->map, header);
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
        {
            .name = "first-sync",
            .val = 'f',
        },
        {
            .name = "help",
            .val = 'h',
        },
        {
            .name = "last",
            .has_arg = required_argument,
            .val = 'l',
        },
        {
            .name = "version",
            .has_arg = no_argument,
            .val = 'z',
        },
        {}
    };
    struct rbh_log_options options = { 0 };
    struct rbh_value_map *logs_map = NULL;
    int rc;
    char c;

    rc = rbh_config_from_args(argc - 1, argv + 1);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to open configuration file");

    while ((c = getopt_long(argc, argv, "c:fhl:z", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'c':
            /* already parsed */
            break;
        case 'f':
            options.ascending = true;
            if (options.count)
                error(EX_USAGE, ENOTSUP,
                      "cannot print logs for both first and last sync");
            options.count = 1;
            break;
        case 'h':
            usage();
            return 0;
        case 'l':
            options.ascending = false;
            if (options.count)
                error(EX_USAGE, ENOTSUP,
                      "cannot print logs for both first and last syncs");

            if (str2uint64_t(optarg, &options.count))
                error(EXIT_FAILURE, errno, "Failed to convert '%s' to uint64_t",
                      optarg);

            break;
        case 'z':
            rbh_print_version();
            return EXIT_SUCCESS;
        case '?':
        default:
            /* getopt_long() prints meaningful error messages itself */
            usage();
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

    logs_map = rbh_backend_get_logs(backend, options);
    if (logs_map == NULL)
        error(EXIT_FAILURE, EINVAL,
              "Failed to retrieve requested logs\n");

    print_logs(logs_map,
               options.ascending ? "First sync" : "Last sync");

    return EXIT_SUCCESS;
}
