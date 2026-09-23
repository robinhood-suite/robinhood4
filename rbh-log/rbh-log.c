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
        "Format output modifiers only work when printing logs, not when\n"
        "deleting them or checking their count. Only one format can be used\n"
        "at a time.\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE                 a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "   -b, --before TIMESTAMP  print logs with that were logged prior to\n"
        "                           TIMESTAMP\n"
        "   -c, --config PATH       the configuration file to use\n"
        "   --csv                   print logs in a CSV format. Only works when\n"
        "                           printing logs, not with '--log-count' or\n"
        "                           '--delete'.\n"
        "   --delete                delete the requested logs instead of printing\n"
        "                           them. Cannot be used with '--log-count',\n"
        "                           and no logs will be printed.\n"
        "   -F, --first             print the first log instead of the last\n"
        "   -h, --help              show this message and exit\n"
        "   --json                  print logs in a JSON format. Only works when\n"
        "                           printing logs, not with '--log-count' or\n"
        "                           '--delete'.\n"
        "   --log-count             print the number of logs currently recorded.\n"
        "                           Cannot be used with '--delete' and no logs\n"
        "                           will be printed.\n"
        "   -n, --count N           print N logs instead of one\n"
        "   --oneline               print logs in a shortened format\n"
        "   -t, --tool TOOLS        print logs of the requested tools instead\n"
        "                           of any tool. TOOLS must be a CSV list\n"
        "                           consisting of the 'rbh' tools 'sync', 'find',\n"
        "                           'report', 'gc', 'fsevents'.\n"
        "    --version              print RobinHood 4's version\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n";

    printf(message, program_invocation_short_name);
}

static void
print_logs(const struct rbh_value_map *logs,
           enum output_format output_format)
{
    for (size_t i = 0 ; i < logs->count ; i++) {
        enum rbh_log_type type = str2rbh_log_type(logs->pairs[i].key);

        switch (output_format) {
        case OF_NORMAL:
            printf("{ rbh-%s:\n", logs->pairs[i].key);
            break;
        case OF_ONELINE:
            printf("{ rbh-%s: ", logs->pairs[i].key);
            break;
        case OF_CSV:
            printf("rbh-%s,", logs->pairs[i].key);
            break;
        case OF_JSON:
            printf("{\n    \"rbh-%s\": {\n", logs->pairs[i].key);
            break;
        }

        switch (type) {
        case RBH_FIND_LOG:
            print_find_log(&logs->pairs[i].value->map, output_format);
            break;
        case RBH_FSEVENTS_LOG:
            print_fsevents_log(&logs->pairs[i].value->map, output_format);
            break;
        case RBH_GC_LOG:
            print_gc_log(&logs->pairs[i].value->map, output_format);
            break;
        case RBH_REPORT_LOG:
            print_report_log(&logs->pairs[i].value->map, output_format);
            break;
        case RBH_SYNC_LOG:
            print_sync_log(&logs->pairs[i].value->map, output_format);
            break;
        default:
            error(EXIT_FAILURE, EINVAL, "Invalid log type retrieved: '%s'",
                  logs->pairs[i].key);
        }

        switch (output_format) {
        case OF_NORMAL:
            printf("}\n");
            break;
        case OF_ONELINE:
            printf(" }\n");
            break;
        case OF_CSV:
            printf("\n");
            break;
        case OF_JSON:
            printf("\n    }\n}\n");
        }
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

static int
parse_tools_list(char *_list, size_t *types)
{
    char *list = xstrdup(_list);
    char *safekeep = list;
    int rc = 0;

    while (list != NULL) {
        enum rbh_log_type type;
        char *comma = strchr(list, ',');

        if (comma == list) {
            rc = 1;
            goto out;
        }

        if (comma)
            *comma = '\0';

        type = str2rbh_log_type(list);
        if (type == RBH_ALL_LOG) {
            rc = 1;
            goto out;
        }

        *types |= type;

        if (comma)
            list = comma + 1;
        else
            break;
    }

out:
    free(safekeep);
    return rc;
}

int
main(int argc, char *argv[])
{
    enum output_format output_format = OF_NORMAL;
    const struct option LONG_OPTIONS[] = {
        {
            .name = "before",
            .has_arg = required_argument,
            .val = 'b',
        },
        {
            .name = "config",
            .has_arg = required_argument,
            .val = 'c',
        },
        {
            .name = "csv",
            .val = 'C',
        },
        {
            .name = "delete",
            .val = 'd',
        },
        {
            .name = "first",
            .val = 'F',
        },
        {
            .name = "help",
            .val = 'h',
        },
        {
            .name = "json",
            .val = 'J',
        },
        {
            .name = "log-count",
            .val = 'Z',
        },
        {
            .name = "count",
            .has_arg = required_argument,
            .val = 'n',
        },
        {
            .name = "oneline",
            .val = 'o',
        },
        {
            .name = "tool",
            .has_arg = required_argument,
            .val = 't',
        },
        {
            .name = "version",
            .val = 'z',
        },
        {}
    };
    struct rbh_log_options options = {
        .type = RBH_ALL_LOG,
        .count = 1
    };
    struct rbh_value_map *logs_map = NULL;
    bool print_count = false;
    bool delete_logs = false;
    int rc;
    char c;

    rc = rbh_config_from_args(argc - 1, argv + 1);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to open configuration file");

    while ((c = getopt_long(argc, argv, "b:c:CdFhjn:ot:zZ",
                            LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'b':
            if (str2uint64_t(optarg, &options.start_timestamp))
                error(EXIT_FAILURE, errno, "Failed to convert '%s' to uint64_t",
                      optarg);

            if (options.start_timestamp == 0)
                error(EXIT_FAILURE, EINVAL,
                      "Cannot print logs of command started before Epoch");

            break;
        case 'c':
            /* already parsed */
            break;
        case 'C':
            if (output_format != OF_NORMAL)
                error(EXIT_FAILURE, EINVAL,
                      "Cannot specify multiple output formats\n");

            output_format = OF_CSV;
            break;
        case 'd':
            delete_logs = true;
            break;
        case 'F':
            options.ascending = true;
            break;
        case 'h':
            usage();
            return 0;
        case 'J':
            if (output_format != OF_NORMAL)
                error(EXIT_FAILURE, EINVAL,
                      "Cannot specify multiple output formats\n");

            output_format = OF_JSON;
            break;
        case 'n':
            if (str2uint64_t(optarg, &options.count))
                error(EXIT_FAILURE, errno, "Failed to convert '%s' to uint64_t",
                      optarg);

            if (options.count == 0)
                error(EXIT_FAILURE, EINVAL, "Cannot print 0 logs");

            break;
        case 'o':
            if (output_format != OF_NORMAL)
                error(EXIT_FAILURE, EINVAL,
                      "Cannot specify multiple output formats\n");

            output_format = OF_ONELINE;
            break;
        case 't':
            if (parse_tools_list(optarg, &options.type))
                error(EXIT_FAILURE, EINVAL,
                      "Failed to parse tools list '%s'", optarg);

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
        int count;

        count = rbh_backend_delete_logs(backend, options);
        if (count < 0) {
            switch (errno) {
            case 0:
                error(EXIT_FAILURE, EINVAL,
                      "Failed to delete requested logs\n");
                break;
            case ENOTSUP:
                error(EXIT_FAILURE, errno,
                      "Failed to delete logs, requested backend doesn't support log deletion\n");
                break;
            case RBH_BACKEND_ERROR:
                error(EXIT_FAILURE, EINVAL,
                      "Failed to delete requested logs: %s\n", rbh_backend_error);
                break;
            default:
                error(EXIT_FAILURE, errno,
                      "Failed to delete requested logs: %s\n", strerror(errno));
                break;
            }
        }

        printf("Deleted '%d' log(s)\n", count);
    } else {
        logs_map = rbh_backend_get_logs(backend, options);
        if (logs_map == NULL)
            error(EXIT_FAILURE, EINVAL,
                  "Failed to retrieve requested logs\n");

        print_logs(logs_map, output_format);
    }

    return EXIT_SUCCESS;
}
