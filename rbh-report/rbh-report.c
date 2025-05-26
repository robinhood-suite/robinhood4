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
#include <robinhood/alias.h>
#include <robinhood/backend.h>
#include <robinhood/config.h>
#include <robinhood/filter.h>
#include <robinhood/filters/parser.h>
#include <robinhood/uri.h>
#include <robinhood/value.h>

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
       bool csv_print, struct rbh_filter *filter)
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

    iter = rbh_backend_report(from, filter, &group, &options, &output);
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
usage(const char *backend)
{
    const char *message =
        "usage: %s [-h | --help] [-c | --config] [-d | --dry-run] SOURCE [--alias NAME] [--csv] [--rsort] [--group-by GROUP-BY] [--output OUTPUT] [PREDICATES]\n"
        "\n"
        "Create a report from SOURCE's entries\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE  a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    --alias NAME          specify an alias for the operation.\n"
        "    --csv                 print the report in CSV format\n"
        "    -d, --dry-run         displays the command after alias management\n"
        "    --group-by GROUP-BY\n"
        "                          the data to group entries on. Can be a CSV\n"
        "                          to group on multiple fields. Fields can\n"
        "                          include a range to create subgroups on\n"
        "                          that field. If not specified, will group\n"
        "                          every entry in one.\n"
        "                          Example: \"statx.size[0;500;10000]\"\n"
        "    -h, --help            show this message and exit\n"
        "    --rsort               reverse sort the output based on the\n"
        "                          grouping requested\n"
        "    -c, --config PATH     the configuration file to use.\n"
        "\n"
        "Output arguments (mandatory):\n"
        "    --output OUTPUT       the information to output. Can be a CSV\n"
        "                          detailling what data to output and the\n"
        "                          order\n"
        "\n"
        "All fields for both grouping and output string should start with the\n"
        "prefix 'statx.' and may be the following:\n"
        "    attributes  atime.nsec  atime.sec   blksize\n"
        "    blocks      btime.nsec  btime.sec\n"
        "    ctime.nsec  ctime.sec   dev.major   dev.minor\n"
        "    gid         ino         mode\n"
        "    mtime.nsec  mtime.sec   nlink\n"
        "    rdev.major  rdev.minor  size\n"
        "    type        uid\n"
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
        "%s"
        "%s"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n"
        "Where:\n"
        "    BACKEND  is the name of a backend\n"
        "    FSNAME   is the name of a filesystem for BACKEND\n"
        "    PATH/ID  is the path/id of an fsentry managed by BACKEND:FSNAME\n"
        "             (ID must be enclosed in square brackets '[ID]' to distinguish it\n"
        "             from a path)\n";

    const struct rbh_backend_plugin *plugin;
    struct rbh_config *config;
    const char *plugin_str;
    char *predicate_helper;
    char *directive_helper;
    int count_chars;

    if (backend == NULL)
        return printf(message, program_invocation_short_name, "", "");

    plugin_str = rbh_config_get_extended_plugin(backend);
    plugin = rbh_backend_plugin_import(plugin_str);
    if (plugin == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_plugin_import");

    config = rbh_config_get();
    rbh_pe_common_ops_helper(plugin->common_ops, backend, config,
                             &predicate_helper, &directive_helper);

    count_chars = printf(message, program_invocation_short_name,
                         predicate_helper ? "Predicate arguments:\n" : "",
                         predicate_helper ? : "");

    free(predicate_helper);
    free(directive_helper);

    return count_chars;
}

void
cleanup(char **others, char *output, char *group)
{
    if (group)
        free(group);
    if (output)
        free(output);
    free(others);
}

static void
check_command_options(int pre_uri_args, int argc, char *argv[])
{
    for (int i = 0; i < pre_uri_args; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            if (i + 1 < pre_uri_args)
                usage(argv[i + 1]);
            else
                usage(NULL);

            exit(0);
        }

        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dry-run") == 0) {
            rbh_display_resolved_argv(program_invocation_short_name, &argc,
                                      &argv);
            exit(0);
        }

        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0)
            i++;
    }
}

int
main(int _argc, char *_argv[])
{
    struct filters_context f_ctx = {0};
    struct rbh_value_map *info_map;
    bool ascending_sort = true;
    struct rbh_filter *filter;
    bool csv_print = false;
    char **others = NULL;
    int others_count = 0;
    char *output = NULL;
    char *group = NULL;
    int nb_cli_args;
    int index = 1;
    char **argv;
    int argc;
    int rc;

    argc = _argc - 1;
    argv = &_argv[1];

    nb_cli_args = rbh_count_args_before_uri(argc, argv);
    rc = rbh_config_from_args(nb_cli_args, argv);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to load configuration file");

    rbh_apply_aliases(&argc, &argv);

    nb_cli_args = rbh_count_args_before_uri(argc, argv);
    check_command_options(nb_cli_args, argc, argv);

    argc = argc - nb_cli_args;
    argv = &argv[nb_cli_args];

    others = malloc(sizeof(char*) * argc);

    for (int i = 0; i < argc; ++i) {
        char *arg = argv[i];

        if (strcmp(arg, "--csv") == 0) {
            csv_print = true;

        } else if (strcmp(arg, "--group-by") == 0) {
            if (i + 1 >= argc)
                error(EXIT_FAILURE, EINVAL, "Missing argument for %s", arg);

            group = strdup(argv[++i]);
            if (!group)
                error(EXIT_FAILURE, ENOMEM, "strdup");

        } else if (strcmp(arg, "--output") == 0) {
            if (i + 1 >= argc)
                error(EXIT_FAILURE, EINVAL, "Missing argument for %s", arg);

            output = strdup(argv[++i]);
            if (!output)
                error(EXIT_FAILURE, ENOMEM, "strdup");

        } else if (strcmp(arg, "--rsort") == 0) {
            ascending_sort = false;

        } else {
            others[others_count++] = arg;
        }
    }

    argc = others_count;
    argv = others;

    if (output == NULL)
        error(EX_USAGE, 0, "missing '--output' argument");
    if (argc < 1)
        error(EX_USAGE, 0, "not enough arguments");
    if (!rbh_is_uri(argv[0]))
        error(EX_USAGE, 0, "There is a filter before the URI");

    from = rbh_backend_from_uri(argv[0], true);

    info_map = rbh_backend_get_info(from, RBH_INFO_BACKEND_SOURCE);
    if (info_map == NULL)
        error(EXIT_FAILURE, errno,
              "Failed to retrieve the source backends from URI '%s', aborting",
              argv[0]);

    import_plugins(&f_ctx, &info_map, 1);
    f_ctx.need_prefetch = false;
    f_ctx.argc = argc;
    f_ctx.argv = argv;

    filter = parse_expression(&f_ctx, &index, NULL, NULL, NULL, false, NULL,
                              NULL);
    if (index != f_ctx.argc)
        error(EX_USAGE, 0, "you have too many ')'");

    const struct rbh_filter_options OPTIONS = {0};
    const struct rbh_filter_output OUTPUT = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = RBH_FP_ALL,
            .statx_mask = RBH_STATX_ALL,
        },
    };

    if (f_ctx.need_prefetch &&
        complete_rbh_filter(filter, from, &OPTIONS, &OUTPUT))
        error(EXIT_FAILURE, errno, "Failed to complete filters");

    report(group, output, ascending_sort, csv_print, filter);

    cleanup(others, output, group);
    filters_ctx_finish(&f_ctx);
    free(filter);

    return EXIT_SUCCESS;
}
