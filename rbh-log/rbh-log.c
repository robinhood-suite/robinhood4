/* This file is part of Robinhood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
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

static int
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
        "    --version              print RobinHood 4's version\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n";

        return printf(message, program_invocation_short_name);
}

#define WIDTH 32

static void
print_sync_log(const struct rbh_value *value, const char *header)
{
    const struct rbh_value_map metadata_map = value->map;
    time_t time;

    assert(value->type == RBH_VT_MAP);

    printf("%s:\n", header);

    for (size_t i = 0 ; i < metadata_map.count ; i++) {
        const struct rbh_value_pair *pair = &metadata_map.pairs[i];
        if (strcmp(pair->key, "sync_debut") == 0) {
            time = (time_t)pair->value->int64;
            printf(" - %-*s %s\n", WIDTH, "Start of the sync:",
                   time_from_timestamp(&time));
        }

        if (strcmp(pair->key, "sync_duration") == 0) {
            char _buffer[32];
            size_t bufsize;
            char *buffer;

            buffer = _buffer;
            bufsize = sizeof(_buffer);

            difftime_printer(buffer, bufsize, pair->value->int64);

            printf(" - %-*s %s\n", WIDTH, "Duration of the sync:",
                   buffer);
        }

        if (strcmp(pair->key, "sync_end") == 0) {
            time = (time_t)pair->value->int64;
            printf(" - %-*s %s\n", WIDTH, "End of the sync:",
                   time_from_timestamp(&time));
        }

        if (strcmp(pair->key, "mountpoint") == 0)
            printf(" - %-*s %s\n", WIDTH, "Mountpoint used for the sync:",
                   pair->value->string);

        if (strcmp(pair->key, "command_line") == 0)
            printf(" - %-*s %s\n", WIDTH, "Command used for the sync:",
                   pair->value->string);

        if (strcmp(pair->key, "converted_entries") == 0)
            printf(" - %-*s %ld\n", WIDTH, "Amount of entries converted:",
                   pair->value->int64);

        if (strcmp(pair->key, "skipped_entries") == 0)
            printf(" - %-*s %ld\n", WIDTH, "Amount of entries skipped:",
                   pair->value->int64);

        if (strcmp(pair->key, "total_entries_seen") == 0)
            printf(" - %-*s %ld\n", WIDTH,
                   "Total entries seen by the sync:",
                   pair->value->int64);
    }
}

static void
print_logs(const struct rbh_value_map *logs, const char *header)
{
    for (size_t i = 0 ; i < logs->count ; i++)
        print_sync_log(logs->pairs[i].value, header);
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
            .name = "last-sync",
            .val = 'l',
        },
        {
            .name = "version",
            .has_arg = no_argument,
            .val = 'z',
        },
        {}
    };
    struct rbh_value_map *logs_map = NULL;
    int flags = 0;
    int rc;
    char c;

    rc = rbh_config_from_args(argc - 1, argv + 1);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to open configuration file");

    while ((c = getopt_long(argc, argv, "c:fhlz", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'c':
            /* already parsed */
            break;
        case 'f':
            flags |= RBH_INFO_FIRST_SYNC;
            break;
        case 'h':
            usage();
            return 0;
        case 'l':
            flags |= RBH_INFO_LAST_SYNC;
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

    if (!flags)
        return EXIT_SUCCESS;

    if (flags & RBH_INFO_FIRST_SYNC && flags & RBH_INFO_LAST_SYNC)
        error(EX_USAGE, ENOTSUP,
              "cannot print logs for both first and last sync");

    backend = rbh_backend_from_uri(argv[0], false);

    logs_map = rbh_backend_get_info(backend, flags);
    if (logs_map == NULL)
        error(EXIT_FAILURE, EINVAL,
              "Failed to retrieve requested logs\n");

    print_logs(logs_map,
               flags & RBH_INFO_FIRST_SYNC ? "First sync" : "Last sync");

    return EXIT_SUCCESS;
}
