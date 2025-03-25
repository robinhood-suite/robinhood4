/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <robinhood.h>
#include <robinhood/backend.h>
#include <robinhood/uri.h>
#include <robinhood/value.h>
#include <robinhood/config.h>
#include <robinhood/alias.h>

#include "report.h"

#define MIN_VALUES_SSTACK_ALLOC (1 << 6)
struct rbh_sstack *values_sstack;

static void __attribute__((destructor))
destroy_values_sstack(void)
{
    if (values_sstack)
        rbh_sstack_destroy(values_sstack);
}

static struct rbh_backend *from;

static void __attribute__((destructor))
destroy_from(void)
{
    const char *name;

    if (from) {
        name = from->name;
        rbh_backend_destroy(from);
        rbh_backend_plugin_destroy(name);
    }
}

static void
report(const char *group_string, const char *output_string, bool ascending_sort,
       bool csv_print)
{
    struct rbh_filter_options options = { 0 };
    struct rbh_filter_output output = { 0 };
    struct rbh_group_fields group = { 0 };
    struct rbh_value_map results[256];
    struct rbh_filter_sort sort = {
        .field = {
            .fsentry = RBH_FP_ID,
        },
        .ascending = ascending_sort,
    };
    struct result_columns columns;
    struct rbh_mut_iterator *iter;
    char _buffer[4096];
    size_t bufsize;
    int count = 0;
    char *buffer;

    if (values_sstack == NULL) {
        values_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                       sizeof(struct rbh_value *));
        if (values_sstack == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "rbh_sstack_new");
    }

    options.sort.items = &sort;
    options.sort.count = 1;

    buffer = _buffer;
    bufsize = sizeof(_buffer);

    parse_group_by(group_string, &group, &columns);
    parse_output(output_string, &group, &output, &columns);

    iter = rbh_backend_report(from, NULL, &group, &options, &output);
    if (iter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_backend_report");

    do {
        struct rbh_value_map *map = rbh_mut_iter_next(iter);

        if (map == NULL)
            break;

        if (group_string == NULL && map->count != 1)
            error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                          "Expected 1 map in output, but found '%ld'",
                          map->count);

        if (group_string != NULL && map->count != 2)
            error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                          "Expected 2 maps in output, but found '%ld'",
                          map->count);

        if (csv_print) {
            csv_print_results(map, group, output);
        } else {
            assert(count < (sizeof(results) / sizeof(results[0])));

            if (value_map_copy(&results[count++], map, &buffer, &bufsize))
                error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                              "Failed to copy result map");
        }

    } while (true);

    if (!csv_print)
        pretty_print_results(results, count, group, output, &columns);
}

/*----------------------------------------------------------------------------*
 |                                    cli                                     |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                              usage()                               |
     *--------------------------------------------------------------------*/

static int
usage(void)
{
    const char *message =
        "usage: %s [-h] SOURCE [-c|--csv] [-r|--rsort] [-g|--group-by GROUP-BY] [-o|--output OUTPUT]\n"
        "\n"
        "Create a report from SOURCE's entries\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE  a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    --alias NAME          specify an alias for the operation.\n"
        "    -c,--csv              print the report in CSV format\n"
        "    -d,--dry-run          displays the command after alias management\n"
        "    -g,--group-by GROUP-BY\n"
        "                          the data to group entries on. Can be a CSV\n"
        "                          to group on multiple fields. Fields can\n"
        "                          include a range to create subgroups on\n"
        "                          that field. If not specified, will group\n"
        "                          every entry in one.\n"
        "                          Example: \"statx.size[0;500;10000]\"\n"
        "    -h,--help             show this message and exit\n"
        "    -r,--rsort            reverse sort the output based on the\n"
        "                          grouping requested\n"
        "    --config PATH         the configuration file to use.\n"
        "\n"
        "Output arguments (mandatory):\n"
        "    -o,--output OUTPUT    the information to output. Can be a CSV\n"
        "                          detailling what data to output and the\n"
        "                          order\n"
        "\n"
        "All fields for both grouping and output string should start with the\n"
        " prefix 'statx.' and may be the following:\n"
        "    blksize     attributes  nlink       uid\n"
        "    gid         type        mode        ino\n"
        "    size        blocks      atime.nsec  atime.sec\n"
        "    btime.nsec  btime.sec   ctime.nsec  ctime.sec\n"
        "    mtime.nsec  mtime.sec   rdev.major  rdev.minor\n"
        "    dev.major   dev.minor   \n"
        "\n"
        "Output info should be the result of an accumulated value, and\n"
        "written as \"<accumulator>(<field>)\" with 'field' one of the above\n"
        "and 'accumulator' one of: 'avg', 'max', 'min', 'sum'. 'count' can\n"
        "also be used as an accumulator, but it doesn't need any field\n"
        "associated.\n"
        "\n"
        "Examples:\n"
        "    rbh-report rbh:mongo:test --output \"max(statx.size),avg(statx.size)\"\n"
        "    rbh-report rbh:mongo:test --group-by \"statx.uid\" --output \"min(statx.ino),count()\"\n"
        "    rbh-report rbh:mongo:test --group-by \"statx.uid,statx.type\" --output \"sum(statx.size),avg(statx.size)\"\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n"
        "Where:\n"
        "    BACKEND  is the name of a backend\n"
        "    FSNAME   is the name of a filesystem for BACKEND\n"
        "    PATH/ID  is the path/id of an fsentry managed by BACKEND:FSNAME\n"
        "             (ID must be enclosed in square brackets '[ID]' to distinguish it\n"
        "             from a path)\n";

    return printf(message, program_invocation_short_name);
}

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "csv",
            .val = 'c',
        },
        {
            .name = "group-by",
            .has_arg = required_argument,
            .val = 'g',
        },
        {
            .name = "help",
            .val = 'h',
        },
        {
            .name = "output",
            .has_arg = required_argument,
            .val = 'o',
        },
        {
            .name = "rsort",
            .val = 'r',
        },
        {
            .name = "dry-run",
            .val = 'd',
        },
        {
            .name = "config",
            .val = 'x',
        },
        {}
    };
    bool ascending_sort = true;
    bool csv_print = false;
    char *output = NULL;
    char *group = NULL;
    char c;

    /* Alias resolution */
    import_configuration_file(&argc, &argv);
    apply_aliases(&argc, &argv);

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "chg:d:o:r",
                            LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'c':
            csv_print = true;
            break;
        case 'g':
            group = strdup(optarg);
            if (group == NULL)
                error(EXIT_FAILURE, ENOMEM, "strdup");
            break;
        case 'h':
            usage();
            return 0;
        case 'o':
            output = strdup(optarg);
            if (output == NULL)
                error(EXIT_FAILURE, ENOMEM, "strdup");
            break;
        case 'r':
            ascending_sort = false;
            break;
        case 'd':
            display_resolved_argv(NULL, &argc, &argv);
            return EXIT_SUCCESS;
        case 'x':
            break;
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
    if (output == NULL)
        error(EX_USAGE, 0, "missing '--output' argument");

    /* Parse SOURCE */
    from = rbh_backend_from_uri(argv[0]);

    report(group, output, ascending_sort, csv_print);

    return EXIT_SUCCESS;
}
