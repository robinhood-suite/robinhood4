/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>

#include <robinhood.h>
#include <robinhood/alias.h>
#include <robinhood/filter.h>
#include <robinhood/filters/parser.h>
#include <robinhood/utils.h>

#include "core.h"

static struct find_context ctx;

static void __attribute__((destructor))
on_find_exit(void)
{
    ctx_finish(&ctx);
}

static int
usage(const char *backend)
{
    const char *message =
        "usage: %s [-h|--help] SOURCE [PREDICATES] [ACTION]\n"
        "\n"
        "Query SOURCE's entries according to PREDICATE and do ACTION on each.\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE  a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    -h,--help             show this message and exit\n"
        "    --alias NAME          specify an alias for the operation.\n"
        "    -d,--dry-run          displays the command after alias management\n"
        "\n"
        "%s"
        "%s"
        "Action arguments:\n"
        "    -count               count the number of entries that match the\n"
        "                         requested predicates\n"
        "    -[r]sort FIELD       sort or reverse sort entries based of the FIELD\n"
        "                         requested\n"
        "\n"
        "%s"
        "%s"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n"
        "Where:\n"
        "    BACKEND  is the name of a backend\n"
        "    FSNAME   the name of the backend instance (a path for a\n"
        "             filesystem, a database name for a database)\n"
        "    PATH/ID  is the path/id of an fsentry managed by BACKEND:FSNAME\n"
        "             (ID must be enclosed in square brackets '[ID]' to distinguish it\n"
        "             from a path)\n"
        "\n"
        "Predicates not implemented yet:\n"
        "    -false         -true\n"
        "    -fstype        -xtype\n"
        "    -readable      -writable    -executable\n"
        "    -iwholename    -wholename\n"
        "    -used\n"
        "    -context\n"
        "\n"
        "Actions not implemented yet:\n"
        "    -prune\n"
        "    -exec COMMANDE {} + -ok COMMANDE ;\n"
        "    -execdir COMMANDE ; -execdir COMMANDE {} + -okdir COMMANDE ;\n";
    const struct rbh_backend_plugin *plugin;
    struct rbh_config *config;
    const char *plugin_str;
    char *predicate_helper;
    char *directive_helper;
    int count_chars;

    if (backend == NULL)
        return printf(message, program_invocation_short_name, "", "", "", "");

    plugin_str = rbh_config_get_extended_plugin(backend);
    plugin = rbh_backend_plugin_import(plugin_str);
    if (plugin == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_plugin_import");

    config = get_rbh_config();
    rbh_pe_common_ops_helper(plugin->common_ops, backend, config,
                             &predicate_helper, &directive_helper);

    count_chars = printf(message, program_invocation_short_name,
                         predicate_helper ? "Predicate arguments:\n" : "",
                         predicate_helper ? : "",
                         directive_helper ? "Printf directives:\n" : "",
                         directive_helper ? : "");

    free(predicate_helper);
    free(directive_helper);

    return count_chars;
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
    struct rbh_filter_sort *sorts = NULL;
    struct rbh_value_map **info_maps;
    struct rbh_filter *filter;
    size_t sorts_count = 0;
    int nb_cli_args;
    char **argv;
    int index;
    int argc;
    int rc;

    if (_argc < 2)
        error(EX_USAGE, EINVAL,
              "invalid number of arguments, expected at least 1");

    argc = _argc - 1;
    argv = &_argv[1];

    nb_cli_args = rbh_find_count_args_before_uri(argc, argv);
    rc = rbh_config_from_args(nb_cli_args, argv);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to load configuration file");

    rbh_apply_aliases(&argc, &argv);

    nb_cli_args = rbh_find_count_args_before_uri(argc, argv);
    check_command_options(nb_cli_args, argc, argv);

    ctx.argc = argc - nb_cli_args;
    ctx.argv = &argv[nb_cli_args];
    ctx.f_ctx.argc = ctx.argc;
    ctx.f_ctx.argv = ctx.argv;

    /* Parse the command line */
    for (index = 0; index < ctx.argc; index++)
        if (str2command_line_token(&ctx.f_ctx, ctx.argv[index], NULL) !=
            CLT_URI)
            break;

    if (index == 0)
        error(EX_USAGE, 0, "missing at least one robinhood URI");

    ctx.backends = malloc(index * sizeof(*ctx.backends));
    if (ctx.backends == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    ctx.uris = malloc(index * sizeof(*ctx.uris));
    if (!ctx.uris)
        error(EXIT_FAILURE, errno, "malloc");

    info_maps = malloc(index * sizeof(*info_maps));
    if (!info_maps)
        error(EXIT_FAILURE, errno, "malloc");

    for (int i = 0; i < index; i++) {
        ctx.backends[i] = rbh_backend_from_uri(ctx.argv[i], true);
        ctx.uris[i] = ctx.argv[i];
        ctx.backend_count++;

        info_maps[i] = rbh_backend_get_info(ctx.backends[i],
                                            RBH_INFO_BACKEND_SOURCE);
        if (info_maps[i] == NULL)
            error(EXIT_FAILURE, errno,
                  "Failed to retrieve the source backends from URI '%s', aborting\n",
                  ctx.argv[i]);
    }

    import_plugins(&ctx.f_ctx, info_maps, ctx.backend_count);
    free(info_maps);

    ctx.f_ctx.need_prefetch = false;
    filter = parse_expression(&ctx.f_ctx, &index, NULL, &sorts, &sorts_count,
                              find_parse_callback, &ctx);
    if (index != ctx.argc)
        error(EX_USAGE, 0, "you have too many ')'");

    if (!ctx.action_done)
        find(&ctx, ACT_PRINT, &index, filter, sorts, sorts_count);
    free(filter);

    return EXIT_SUCCESS;
}
