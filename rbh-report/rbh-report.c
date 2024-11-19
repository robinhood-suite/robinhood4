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

#define MIN_VALUES_SSTACK_ALLOC (1 << 6)
static __thread struct rbh_sstack *values_sstack;

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
create_rbh_group_fields(struct rbh_group_fields *group)
{
    struct rbh_modifier_field *fields;

    fields = rbh_sstack_push(values_sstack, NULL, 1 * sizeof(*fields));
    if (fields == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_push");

    fields[0].modifier = FM_SUM;
    fields[0].field.fsentry = RBH_FP_STATX;
    fields[0].field.statx = RBH_STATX_SIZE;

    group->fields = fields;
    group->count = 1;
}

static void
create_rbh_filter_output(struct rbh_filter_output *output)
{
    struct rbh_value_pair *pairs;
    struct rbh_value *value;

    pairs = rbh_sstack_push(values_sstack, NULL, 1 * sizeof(*pairs));
    if (pairs == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_push");

    value = rbh_sstack_push(values_sstack, NULL, 1 * sizeof(*value));
    if (value == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_push");

    pairs[0].key = "result";
    pairs[0].value = value;
    value->type = RBH_VT_STRING;
    /** XXX: this is not really generic, but it'll do for now */
    value->string = "$test";
    output->type = RBH_FOT_MAP;
    output->map.pairs = pairs;
    output->map.count = 1;
}

static void
report()
{
    struct rbh_filter_options options = { 0 };
    struct rbh_filter_output output = { 0 };
    struct rbh_group_fields group = { 0 };
    struct rbh_mut_iterator *iter;

    if (values_sstack == NULL) {
        values_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                       sizeof(struct rbh_value *));
        if (values_sstack == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "rbh_sstack_new");
    }

    create_rbh_filter_output(&output);
    create_rbh_group_fields(&group);
    iter = rbh_backend_report(from, NULL, &group, &options, &output);

    if (iter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_backend_report");

    do {
        struct rbh_value_map *map = rbh_mut_iter_next(iter);

        if (map == NULL || map->count != 1 ||
            strcmp(map->pairs[0].key, "result") != 0 ||
            map->pairs[0].value->type != RBH_VT_INT64)
            break;

        printf("%ld\n", map->pairs[0].value->int64);
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
        "usage: %s [-h] SOURCE\n"
        "\n"
        "Create a report from SOURCE's entries\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE  a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    -h,--help             show this message and exit\n"
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
            .name = "help",
            .val = 'h',
        },
        {}
    };
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "h", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'h':
            usage();
            return 0;
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

    /* Parse SOURCE */
    from = rbh_backend_from_uri(argv[0]);

    report();

    return EXIT_SUCCESS;
}
