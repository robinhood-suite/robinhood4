/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
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
dump_value(const struct rbh_value *value)
{
    switch (value->type) {
    case RBH_VT_INT32:
        printf("%d", value->int32);
        break;
    case RBH_VT_INT64:
        printf("%ld", value->int64);
        break;
    case RBH_VT_STRING:
        printf("%s", value->string);
        break;
    case RBH_VT_SEQUENCE:
        printf("[");
        for (int j = 0; j < value->sequence.count; j++) {
            dump_value(&value->sequence.values[j]);
            if (j < value->sequence.count - 1)
                printf("; ");
        }
        printf("]");
        break;
    default:
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected value type, found '%s'",
                      VALUE_TYPE_NAMES[value->type]);
    }
}

static void
dump_map(const struct rbh_value_map *map, int expected_count, const char *key)
{
    if (map->count != expected_count)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected number of fields in '%s' map, expected '%d', got '%ld'",
                      key, expected_count, map->count);

    for (int i = 0; i < map->count; i++) {
        dump_value(map->pairs[i].value);

        if (i < map->count - 1)
            printf(",");
    }
}

static void
report(const char *group_string, const char *output_string, bool ascending_sort)
{
    struct rbh_filter_options options = { 0 };
    struct rbh_filter_output output = { 0 };
    struct rbh_group_fields group = { 0 };
    struct rbh_filter_sort sort = {
        .field = {
            .fsentry = RBH_FP_ID,
        },
        .ascending = ascending_sort,
    };
    struct rbh_mut_iterator *iter;

    if (values_sstack == NULL) {
        values_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                       sizeof(struct rbh_value *));
        if (values_sstack == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "rbh_sstack_new");
    }

    fill_group_by_fields(group_string, &group);
    fill_acc_and_output_fields(output_string, &group, &output);

    options.sort.items = &sort;
    options.sort.count = 1;

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

        if (map->count == 2) {
            dump_map(&map->pairs[0].value->map, group.id_count, "id");
            printf(": ");
            dump_map(&map->pairs[1].value->map, output.output_fields.count,
                     "output");
        } else {
            dump_map(&map->pairs[0].value->map, output.output_fields.count,
                     "output");
        }

        printf("\n");
    } while (true);
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
        "usage: %s [-h] SOURCE [-o|--output OUTPUT]\n"
        "\n"
        "Create a report from SOURCE's entries\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE  a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    -h,--help             show this message and exit\n"
        "\n"
        "Output arguments:\n"
        "    -o,--output OUTPUT    the information to output. Can be a CSV\n"
        "                          detailling what data to output and the\n"
        "                          order\n"
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
        {}
    };
    bool ascending_sort = true;
    char *output = NULL;
    char *group = NULL;
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "hg:o:", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
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

    report(group, output, ascending_sort);

    return EXIT_SUCCESS;
}
