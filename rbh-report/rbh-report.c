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
       bool csv_print, struct rbh_filter *filter,
       struct rbh_filter_options *options)
{
    struct rbh_filter_output output = { 0 };
    struct rbh_sstack *buffer_sstack = NULL;
    struct rbh_group_fields group = { 0 };
    struct rbh_list_node *results = NULL;
    struct rbh_filter_sort sort = {
        .field = {
            .fsentry = RBH_FP_ID,
        },
        .ascending = ascending_sort,
    };
    struct result_columns columns;
    struct rbh_mut_iterator *iter;
    size_t buf_size = 4096;
    struct map_node *node;
    char *buffer;
    size_t size;

    if (values_sstack == NULL)
        values_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                       sizeof(struct rbh_value *));

    if (!csv_print) {
        buffer_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC * buf_size);

        results = xmalloc(sizeof(*results));

        rbh_list_init(results);
    }

    options->sort.items = &sort;
    options->sort.count = 1;

    parse_group_by(group_string, &group, &columns);
    parse_output(output_string, &group, &output, &columns);

    iter = rbh_backend_report(from, filter, &group, options, &output);
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
            node = xmalloc(sizeof(*node));

            buffer = RBH_SSTACK_PUSH(buffer_sstack, NULL, buf_size);
            size = buf_size;
            if (value_map_copy(&node->map, map, &buffer, &size))
                error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                              "Failed to copy result map");
            rbh_list_add_tail(results, &node->link);
        }

    } while (true);

    rbh_mut_iter_destroy(iter);

    if (!csv_print) {
        pretty_print_results(results, group, output, &columns);

        rbh_list_del(results);
        free(results);
        rbh_sstack_destroy(buffer_sstack);
    }
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
        "Usage: %s [PRE_URI_OPTIONS] SOURCE [POST_URI_OPTIONS] [--output OUTPUT] [PREDICATES]\n"
        "\n"
        "Create a report from SOURCE's entries\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE                a robinhood URI\n"
        "\n"
        "Pre URI optional arguments:\n"
        "    -c, --config PATH     the configuration file to use.\n"
        "    -d, --dry-run         displays the command after alias management\n"
        "    -h, --help            show this message and exit\n"
        "    -v, --verbose         show additionnal information\n"
        "\n"
        "Post URI optional arguments:\n"
        "    --alias NAME          specify an alias for the operation.\n"
        "    --csv                 print the report in CSV format\n"
        "    --group-by GROUP-BY\n"
        "                          the data to group entries on. Can be a CSV\n"
        "                          to group on multiple fields. Fields can\n"
        "                          include a range to create subgroups on\n"
        "                          that field. If not specified, will group\n"
        "                          every entry in one.\n"
        "                          Example: \"statx.size[0;500;10000]\"\n"
        "    --rsort               reverse sort the output based on the\n"
        "                          grouping requested\n"
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
get_command_options(int argc, char *argv[], struct command_context *context)
{
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            context->helper = true;
            if (i + 1 < argc)
                context->helper_target = argv[i + 1];
        }

        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dry-run") == 0)
            context->dry_run = true;

        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc)
                error(EXIT_FAILURE, EINVAL,
                      "missing configuration file value");

            context->config_file = argv[i + 1];
        }

        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            context->verbose = true;
    }
}

static void
apply_command_options(struct command_context *context, int argc, char *argv[])
{
    if (context->helper) {
        if (context->helper_target)
            usage(context->helper_target);
        else
            usage(NULL);

        exit(0);
    }

    if (context->dry_run) {
        rbh_display_resolved_argv(program_invocation_short_name, &argc, &argv);
        if (!context->verbose)
            exit(0);
    }
}


int
main(int _argc, char *_argv[])
{
    struct command_context command_context = {0};
    struct rbh_filter_options options = {0};
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

    argc = _argc - 1;
    argv = &_argv[1];

    nb_cli_args = rbh_count_args_before_uri(argc, argv);
    get_command_options(nb_cli_args, argv, &command_context);

    rbh_config_load_from_path(command_context.config_file);
    rbh_apply_aliases(&argc, &argv);

    nb_cli_args = rbh_count_args_before_uri(argc, argv);
    get_command_options(nb_cli_args, argv, &command_context);
    apply_command_options(&command_context, argc, argv);

    argc = argc - nb_cli_args;
    argv = &argv[nb_cli_args];
    options.verbose = command_context.verbose;
    options.dry_run = command_context.dry_run;

    others = xmalloc(sizeof(char*) * argc);

    for (int i = 0; i < argc; ++i) {
        char *arg = argv[i];

        if (strcmp(arg, "--csv") == 0) {
            csv_print = true;

        } else if (strcmp(arg, "--group-by") == 0) {
            if (i + 1 >= argc)
                error(EXIT_FAILURE, EINVAL, "Missing argument for %s", arg);

            group = xstrdup(argv[++i]);

        } else if (strcmp(arg, "--output") == 0) {
            if (i + 1 >= argc)
                error(EXIT_FAILURE, EINVAL, "Missing argument for %s", arg);

            output = xstrdup(argv[++i]);

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

    filter = parse_expression(&f_ctx, &index, NULL, NULL, NULL, NULL);
    if (index != f_ctx.argc)
        error(EX_USAGE, 0, "you have too many ')'");

    if (f_ctx.need_prefetch && complete_rbh_filter(filter, from, &options))
        error(EXIT_FAILURE, errno, "Failed to complete filters");

    report(group, output, ascending_sort, csv_print, filter, &options);

    cleanup(others, output, group);
    filters_ctx_finish(&f_ctx);
    free(filter);

    return EXIT_SUCCESS;
}
