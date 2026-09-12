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
        "Usage: %s SOURCE [OPTIONS]\n"
        "\n"
        "Print logs from SOURCE's metadata.\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE                 a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "   -c, --config PATH       the configuration file to use\n"
        "   --count                 print the number of logs currently recorded.\n"
        "                           Cannot be used with '--delete'\n"
        "   --delete                delete the requested logs instead of printing\n"
        "                           them. Cannot be used with '--count'\n"
        "   -h, --help              show this message and exit\n"
        "   -i, --find              print rbh-find logs\n"
        "   -f, --fsevents          print rbh-fsevents logs\n"
        "   -F, --first             print the first log instead of the last\n"
        "   -g, --gc                print rbh-gc logs\n"
        "   -n N                    print N logs\n"
        "   --oneline               print logs in a shortened format\n"
        "   -r, --report            print rbh-report logs\n"
        "   -s, --sync              print rbh-sync logs\n"
        "    --version              print RobinHood 4's version\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n";

    printf(message, program_invocation_short_name);
}

static void
print_logs(const struct rbh_value_map *logs, bool print_oneline)
{
    for (size_t i = 0 ; i < logs->count ; i++) {
        enum rbh_log_type type = str2rbh_log_type(logs->pairs[i].key);

        printf("{ rbh-%s:%s", logs->pairs[i].key, print_oneline ? " " : "\n");

        switch (type) {
        case RBH_FIND_LOG:
            print_find_log(&logs->pairs[i].value->map, print_oneline);
            break;
        case RBH_FSEVENTS_LOG:
            print_fsevents_log(&logs->pairs[i].value->map, print_oneline);
            break;
        case RBH_GC_LOG:
            print_gc_log(&logs->pairs[i].value->map, print_oneline);
            break;
        case RBH_REPORT_LOG:
            print_report_log(&logs->pairs[i].value->map, print_oneline);
            break;
        case RBH_SYNC_LOG:
            print_sync_log(&logs->pairs[i].value->map, print_oneline);
            break;
        default:
            error(EXIT_FAILURE, EINVAL, "Invalid log type retrieved: '%s'",
                  logs->pairs[i].key);
        }

        printf("%s}\n", print_oneline ? " " : "");
    }
}

static void
print_log_count(const struct rbh_value_map *counts)
{
    int64_t count = 0;

    for (size_t i = 0 ; i < counts->count ; i++) {
        printf("Log count for the '%s' command: '%ld'\n",
               counts->pairs[i].key, counts->pairs[i].value->int64);
        count += counts->pairs[i].value->int64;
    }

    printf("Total log count: '%ld'\n", count);
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
            .name = "count",
            .val = 'Z',
        },
        {
            .name = "delete",
            .val = 'd',
        },
        {
            .name = "find",
            .val = 'i',
        },
        {
            .name = "fsevents",
            .val = 'f',
        },
        {
            .name = "first",
            .val = 'F',
        },
        {
            .name = "gc",
            .val = 'g',
        },
        {
            .name = "help",
            .val = 'h',
        },
        {
            .name = "n",
            .has_arg = required_argument,
            .val = 'n',
        },
        {
            .name = "oneline",
            .val = 'o',
        },
        {
            .name = "report",
            .val = 'r',
        },
        {
            .name = "sync",
            .val = 's',
        },
        {
            .name = "version",
            .val = 'z',
        },
        {}
    };
    struct rbh_log_options options = { .type = RBH_ALL_LOG };
    struct rbh_value_map *logs_map = NULL;
    bool print_oneline = false;
    bool print_count = false;
    bool delete_logs = false;
    int rc;
    char c;

    rc = rbh_config_from_args(argc - 1, argv + 1);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to open configuration file");

    while ((c = getopt_long(argc, argv, "c:difFghn:orszZ",
                            LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'c':
            /* already parsed */
            break;
        case 'd':
            delete_logs = true;
            break;
        case 'i':
            options.type = RBH_FIND_LOG;
            break;
        case 'f':
            options.type = RBH_FSEVENTS_LOG;
            break;
        case 'F':
            options.ascending = true;
            break;
        case 'g':
            options.type = RBH_GC_LOG;
            break;
        case 'h':
            usage();
            return 0;
        case 'n':
            if (str2uint64_t(optarg, &options.count))
                error(EXIT_FAILURE, errno, "Failed to convert '%s' to uint64_t",
                      optarg);

            if (options.count == 0)
                error(EXIT_FAILURE, EINVAL, "Cannot print 0 logs");

            break;
        case 'o':
            print_oneline = true;
            break;
        case 'r':
            options.type = RBH_REPORT_LOG;
            break;
        case 's':
            options.type = RBH_SYNC_LOG;
            break;
        case 'z':
            rbh_print_version();
            return EXIT_SUCCESS;
        case 'Z':
            print_count = true;
            break;
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
    if (print_count && delete_logs)
        error(EX_USAGE, 0,
              "Cannot both print the log count and delete logs. Choose one.");

    backend = rbh_backend_from_uri(argv[0], false);

    if (print_count) {
        logs_map = rbh_backend_get_log_count(backend);
        if (logs_map == NULL)
            error(EXIT_FAILURE, EINVAL,
                  "Failed to retrieve log count\n");

        print_log_count(logs_map);
    } else if (delete_logs) {
        if (options.count == 0)
            error(EXIT_FAILURE, EINVAL,
                  "Cannot delete 0 logs, specify a type and count to delete\n");

        if (rbh_backend_delete_logs(backend, options))
            error(EXIT_FAILURE, EINVAL,
                  "Failed to delete requested logs\n");
    } else {
        logs_map = rbh_backend_get_logs(backend, options);
        if (logs_map == NULL)
            error(EXIT_FAILURE, EINVAL,
                  "Failed to retrieve requested logs\n");

        print_logs(logs_map, print_oneline);
    }

    return EXIT_SUCCESS;
}
