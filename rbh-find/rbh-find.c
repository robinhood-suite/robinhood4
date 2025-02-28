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
#include <robinhood/utils.h>
#include <robinhood/alias.h>

#include "rbh-find/actions.h"
#include "rbh-find/core.h"
#include "rbh-find/filters.h"
#include "rbh-find/find_cb.h"
#include "rbh-find/parser.h"

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
        "usage: %s [-h|--help] SOURCE [PREDICATES] [ACTION]\n"
        "\n"
        "Query SOURCE's entries according to PREDICATE and do ACTION on each.\n"
        "\n"
        "Most of the predicates and actions are similar to the ones of GNU's find,\n"
        "so we will only list the differences here.\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE  a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    -h,--help             show this message and exit\n"
        "    --alias NAME          specify an alias for the operation.\n"
        "    -d,--dry-run          displays the command after alias management\n"
        "\n"
        "Predicate arguments:\n"
        "    -[acm]min [+-]TIME   filter entries based on their access,\n"
        "                         change or modify time. TIME should represent\n"
        "                         minutes, and the filtering will follow GNU's\n"
        "                         find logic for '-[acm]time'\n"
        "    -empty               filter entries that are empty and is either\n"
        "                         a regular file or a directory. Works only\n"
        "                         with regular files for now\n"
        "    -gid GID             filter entries based on their owner's GID\n"
        "    -group GROUP         filter entries based on their owner's group name\n"
        "    -nogroup             select entries without valid owner group IDs\n"
        "    -nouser              select entries without valid owner IDs\n"
        "    -size [+-]SIZE       filter entries based of their size. Works like\n"
        "                         GNU find's '-size' predicate except with the\n"
        "                         addition of the 'T' modifier for terabytes\n"
        "    -perm PERMISSIONS    filter entries based on their permissions,\n"
        "                         the '+' prefix is not supported\n"
        "    -uid UID             filter entries based on their owner's ID\n"
        "    -user USER           filter entries based on their owner's name\n"
        "\n"
        "Action arguments:\n"
        "    -count               count the number of entries that match the\n"
        "                         requested predicates\n"
        "    -[r]sort FIELD       sort or reverse sort entries based of the FIELD\n"
        "                         requested\n"
        "\n"
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
        "    -iwholename    -ilname      -wholename     -lname\n"
        "    -inum\n"
        "    -used\n"
        "    -context\n"
        "\n"
        "Actions not implemented yet:\n"
        "    -prune\n"
        "    -exec COMMANDE {} + -ok COMMANDE ;\n"
        "    -execdir COMMANDE ; -execdir COMMANDE {} + -okdir COMMANDE ;\n";

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
            display_resolved_argv(program_invocation_short_name, &argc, &argv);
            exit(0);
        }

        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
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
    char **argv;
    int index;
    int argc;

    if (_argc < 2)
        error(EX_USAGE, EINVAL,
              "invalid number of arguments, expected at least 1");

    argc = _argc - 1;
    argv = &_argv[1];

    import_configuration_file(&argc, &argv);
    apply_aliases(&argc, &argv);
    checked_options = check_command_options(argc, argv);

    ctx.argc = argc - checked_options;
    ctx.argv = &argv[checked_options];

    ctx.pre_action_callback = &find_pre_action;
    ctx.exec_action_callback = &find_exec_action;
    ctx.post_action_callback = &find_post_action;
    ctx.parse_predicate_callback = &find_parse_predicate;
    ctx.pred_or_action_callback = &find_predicate_or_action;
    ctx.print_directive = &fsentry_print_directive;

    /* Parse the command line */
    for (index = 0; index < ctx.argc; index++) {
        if (str2command_line_token(&ctx, ctx.argv[index]) != CLT_URI)
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
    ctx.need_prefetch = false;
    filter = parse_expression(&ctx, &index, NULL, &sorts, &sorts_count);
    if (index != ctx.argc)
        error(EX_USAGE, 0, "you have too many ')'");

    if (!ctx.action_done)
        find(&ctx, ACT_PRINT, &index, filter, sorts, sorts_count);
    free(filter);

    return EXIT_SUCCESS;
}
