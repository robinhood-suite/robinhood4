/* This file is part of RobinHood 4
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
#include <string.h>
#include <sysexits.h>

#include <robinhood.h>
#include <robinhood/utils.h>
#include <robinhood/config.h>
#include <robinhood/alias.h>

#include <rbh-find/actions.h>
#include <rbh-find/core.h>
#include <rbh-find/find_cb.h>

static struct find_context ctx;

static void __attribute__((destructor))
on_find_exit(void)
{
    ctx_finish(&ctx);
}

static int
usage(void)
{
    const char *message =
        "usage: %s [-h|--help] [-c|--config] SOURCE [PREDICATES] [ACTION]\n"
        "\n"
        "Query SOURCE's entries according to PREDICATE and do ACTION on each.\n"
        "This command supports all of rbh-find's predicates and actions, plus new\n"
        "ones specific to Lustre.\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE               a robinhood URI.\n"
        "\n"
        "Optional arguments:\n"
        "    -c, --config PATH    the configuration file to use.\n"
        "    -h, --help           show this message and exit.\n"
        "    --alias NAME         specify an alias for the operation.\n"
        "    -d, --dry-run        displays the command after alias management\n"
        "\n"
        "Predicate arguments:\n"
        "    -fid FID             filter entries based on their FID.\n"
        "    -hsm-state {archived, dirty, exists, lost, noarchive, none, norelease, released}\n"
        "                         filter entries based on their HSM state.\n"
        "    -ost-index INDEX     filter entries based on the OST they are on.\n"
        "    -layout-pattern {default, raid0, released, mdt, overstriped}\n"
        "                         filter entries based on the layout pattern\n"
        "                         of their components. If given default, will\n"
        "                         fetch the default pattern of the current\n"
        "                         Lustre FS and use it for filtering.\n"
        "    -mdt-index INDEX     filter entries based on the MDT they are on.\n"
        "    -pool NAME           filter entries based on the pool their\n"
        "                         components belong to (case sensitive, regex\n"
        "                         allowed).\n"
        "    -ipool NAME          filter entries based on the pool their\n"
        "                         components belong to (case insensitive,\n"
        "                         regex allowed).\n"
        "    -stripe-count {[+-]COUNT, default}\n"
        "                         filter entries based on their component's\n"
        "                         stripe count. If given default, will fetch\n"
        "                         the default stripe count of the current\n"
        "                         Lustre FS and use it for filtering.\n"
        "    -stripe-size {[+-]SIZE, default}\n"
        "                         filter entries based on their component's\n"
        "                         stripe size. If given default, will fetch\n"
        "                         the default stripe size of the current\n"
        "                         Lustre FS and use it for filtering.\n"
        "    -expired-at {[+-]EPOCH, inf}\n"
        "                         filter entries based on if they expired\n"
        "                         before the given epoch (using `-`), before\n"
        "                         and at the given epoch (using no `+` or `-`\n"
        "                         sign, or after the given epoch (using `+`).\n"
        "                         If given `inf`, will filter entries that\n"
        "                         have an infinite expiration date.\n"
        "    -expired             filter entries based on if they're expired\n"
        "                         at the time of command's invocation.\n"
        "    -comp-start [+-]SIZE[,SIZE]\n"
        "                         filter entries based on their component's\n"
        "                         start values. `+` or `-` signs are\n"
        "                         not considered if given an interval in CSV.\n"
        "    -comp-end   [+-]SIZE[,SIZE]\n"
        "                         filter entries based on their component's\n"
        "                         end values. `+` or `-` signs are\n"
        "                         not considered if given an interval in CSV.\n"
        "    -mdt-count  [+-]COUNT\n"
        "                         filter entries based on their MDT count.\n"
        "\n"
        "Action arguments:\n"
        "    -printf STRING       output entries' information by following\n"
        "                         the pattern given by the input string.\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n"
        "Where:\n"
        "    BACKEND  is the name of a backend.\n"
        "    FSNAME   is the name of the backend instance (a path for a\n"
        "             filesystem, a database name for a database).\n"
        "    PATH/ID  is the path/id of an fsentry managed by BACKEND:FSNAME\n"
        "             (ID must be enclosed in square brackets '[ID]' to distinguish it\n"
        "             from a path).\n";

    return printf(message, program_invocation_short_name);
}

static int
check_command_options(int argc, char *argv[])
{
    for (int i = 0; i < argc; i++) {
        if (*argv[i] != '-')
            return i;

        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage();
            exit(0);
        }

        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dry-run") == 0) {
            rbh_display_resolved_argv(program_invocation_short_name, &argc,
                                      &argv);
            exit(0);
        }

        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            i++;
        }

        if (strcmp(argv[i], "--alias") == 0) {
            i++;
        }
    }

    return 0;
}

int
main(int _argc, char *_argv[])
{
    struct rbh_filter_sort *sorts = NULL;
    struct rbh_filter *filter;
    size_t sorts_count = 0;
    int checked_options;
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
    checked_options = check_command_options(argc, argv);

    ctx.argc = argc - checked_options;
    ctx.argv = &argv[checked_options];

    ctx.pre_action_callback = &find_pre_action;
    ctx.exec_action_callback = &find_exec_action;
    ctx.post_action_callback = &find_post_action;

    /* Parse the command line */
    for (index = 0; index < ctx.argc; index++) {
        if (str2command_line_token(&ctx, ctx.argv[index], NULL) != CLT_URI)
            break;
    }
    if (index == 0)
        error(EX_USAGE, 0, "missing at least one robinhood URI");

    ctx.backends = malloc(index * sizeof(*ctx.backends));
    if (ctx.backends == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    ctx.uris = malloc(index * sizeof(*ctx.uris));
    if (!ctx.uris)
        error(EXIT_FAILURE, errno, "malloc");

    for (int i = 0; i < index; i++) {
        ctx.backends[i] = rbh_backend_from_uri(ctx.argv[i]);
        ctx.uris[i] = ctx.argv[i];
        ctx.backend_count++;
    }

    ctx.info_pe_count = 2;
    ctx.info_pe = malloc(ctx.info_pe_count * sizeof(*ctx.info_pe));
    if (ctx.info_pe == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    ctx.info_pe[0].is_plugin = true;
    ctx.info_pe[0].plugin = rbh_backend_plugin_import("posix");
    if (ctx.info_pe[0].plugin == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_plugin_import");

    ctx.info_pe[1].is_plugin = false;
    ctx.info_pe[1].extension = rbh_plugin_load_extension(
        &ctx.info_pe[0].plugin->plugin,
        "lustre"
    );
    if (ctx.info_pe[1].extension == NULL)
        error(EXIT_FAILURE, errno, "rbh_plugin_load_extension");

    filter = parse_expression(&ctx, &index, NULL, &sorts, &sorts_count);
    if (index != ctx.argc)
        error(EX_USAGE, 0, "you have too many ')'");

    if (!ctx.action_done)
        find(&ctx, ACT_PRINT, &index, filter, sorts, sorts_count);
    free(filter);

    rbh_config_free();

    return EXIT_SUCCESS;
}
