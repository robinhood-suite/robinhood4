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

static enum field_modifier
str2modifier(const char *str)
{
    switch (*str++) {
    case 'a': /* avg */
        if (strcmp(str, "vg"))
            break;

        return FM_AVG;
    case 's': /* sum */
        if (strcmp(str, "um"))
            break;

        return FM_SUM;
    }

    error(EX_USAGE, 0, "invalid modifier '%s'", str);
    __builtin_unreachable();
}

static const struct rbh_modifier_field
convert_output_string_to_modifier_field(const char *output_string)
{
    const struct rbh_filter_field *filter_field;
    struct rbh_modifier_field field;
    char *opening;
    char *str;

    str = strdup(output_string);
    if (str == NULL)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__, "strdup");

    opening = strchr(str, '(');
    if (opening) {
        char *closing = strchr(str, ')');

        if (closing == NULL)
            error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                          "'%s' ill-formed, missing ')'", output_string);

        *opening = '\0';
        *closing = '\0';
        field.modifier = str2modifier(str);
    } else {
        opening = str;
        field.modifier = FM_NONE;
    }

    filter_field = str2filter_field(opening + 1);
    if (filter_field == NULL)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "'%s' ill-formed, invalid field", output_string);

    field.field = *filter_field;
    free(str);

    return field;
}

static void
create_rbh_filter_group_output(const char *output_string,
                               struct rbh_group_fields *group,
                               struct rbh_filter_output *output)
{
    struct rbh_modifier_field *fields;

    fields = rbh_sstack_push(values_sstack, NULL, 1 * sizeof(*fields));
    if (fields == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_push");

    fields[0] = convert_output_string_to_modifier_field(output_string);

    group->fields = fields;
    group->count = 1;
    output->type = RBH_FOT_VALUES;
    output->output_fields.fields = fields;
    output->output_fields.count = 1;
}

static void
report(const char *output_string)
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

    create_rbh_filter_group_output(output_string, &group, &output);
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
            .name = "help",
            .val = 'h',
        },
        {
            .name = "output",
            .has_arg = required_argument,
            .val = 'o',
        },
        {}
    };
    char *output = NULL;
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "ho:", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'h':
            usage();
            return 0;
        case 'o':
            output = strdup(optarg);
            if (output == NULL)
                error(EXIT_FAILURE, ENOMEM, "strdup");
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

    report(output);

    return EXIT_SUCCESS;
}
