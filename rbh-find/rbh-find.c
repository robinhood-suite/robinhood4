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
        "    -blocks [+-]N        filter entries based on their number of blocks\n"
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
        "    -iwholename    -wholename\n"
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
            rbh_display_resolved_argv(program_invocation_short_name, &argc,
                                      &argv);
            exit(0);
        }

        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0)
            i++;
    }

    return 0;
}

static int
import_backend_source(int pe_count, const struct rbh_value_map *backend_source)
{
    return 0;
}

/* XXX: Ugly but necessary until we can retrieve the list of backends
 * used to construct the mirror system from the mirror system itself.
 */
static void
import_plugins(bool mongo_found, bool mpi_found,
               struct rbh_value_map **info_maps)
{
    int pe_count = 0;
    int pe_index = 0;

    for (int i = 0; i < ctx.backend_count; ++i) {
        assert(info_maps[i]->count == 1);
        assert(strcmp(info_maps[i]->pairs->key, "backend_source") == 0);
        assert(info_maps[i]->pairs[0].value->type == RBH_VT_SEQUENCE);

        ctx.info_pe_count += info_maps[i]->pairs[0].value->sequence.count;
    }

    ctx.info_pe = malloc(ctx.info_pe_count * sizeof(*ctx.info_pe));
    if (ctx.info_pe == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    pe_count = 0;

    for (int i = 0; i < ctx.backend_count; ++i) {
        const struct rbh_value *backend_source_sequence =
            info_maps[i]->pairs[0].value;

        for (int j = 0; j < backend_source_sequence->sequence.count; ++j) {
            const struct rbh_value *backend_source =
                &backend_source_sequence->sequence.values[j];

            assert(backend_source->type == RBH_VT_MAP);
            pe_count += import_backend_source(pe_count, &backend_source->map);
        }
    }

    ctx.info_pe_count = pe_count;

    // XXX: temporary, will be removed
    free(ctx.info_pe);
    ctx.info_pe_count = 0;

    if (mongo_found)
        ctx.info_pe_count++;
    else if (mpi_found)
        ctx.info_pe_count++;

    ctx.info_pe = malloc(ctx.info_pe_count * sizeof(*ctx.info_pe));
    if (ctx.info_pe == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    if (mongo_found) {
        ctx.info_pe[pe_index].plugin = rbh_backend_plugin_import("posix");
        if (ctx.info_pe[pe_index].plugin == NULL)
            error(EXIT_FAILURE, errno, "rbh_backend_plugin_import");

        ctx.info_pe[pe_index].is_plugin = true;
        pe_index++;
    }

    if (mpi_found) {
        ctx.info_pe[pe_index].plugin = rbh_backend_plugin_import("mpi-file");
        if (ctx.info_pe[pe_index].plugin == NULL)
            error(EXIT_FAILURE, errno, "rbh_backend_plugin_import");

        ctx.info_pe[pe_index].is_plugin = true;
    }
}

int
main(int _argc, char *_argv[])
{
    struct rbh_filter_sort *sorts = NULL;
    struct rbh_value_map **info_maps;
    struct rbh_filter *filter;
    bool mongo_found = false;
    bool mpi_found = false;
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
    for (index = 0; index < ctx.argc; index++)
        if (str2command_line_token(&ctx, ctx.argv[index], NULL) != CLT_URI)
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
        ctx.backends[i] = rbh_backend_from_uri(ctx.argv[i]);
        ctx.uris[i] = ctx.argv[i];
        ctx.backend_count++;

        info_maps[i] = rbh_backend_get_info(ctx.backends[i],
                                            RBH_INFO_BACKEND_SOURCE);
        if (info_maps[i] == NULL)
            error(EXIT_FAILURE, errno,
                  "Failed to retrieve the source backends from URI '%s', aborting\n",
                  ctx.argv[i]);

        /* XXX: Ugly but necessary until we can retrieve the list of backends
         * used to construct the mirror system from the mirror system itself.
         */
        if (!mongo_found || strcmp(ctx.backends[i]->name, "mongo") == 0)
            mongo_found = true;
        else if (!mpi_found || strcmp(ctx.backends[i]->name, "mpi-file") == 0)
            mpi_found = true;
    }

    import_plugins(mongo_found, mpi_found, info_maps);

    ctx.need_prefetch = false;
    filter = parse_expression(&ctx, &index, NULL, &sorts, &sorts_count);
    if (index != ctx.argc)
        error(EX_USAGE, 0, "you have too many ')'");

    if (!ctx.action_done)
        find(&ctx, ACT_PRINT, &index, filter, sorts, sorts_count);
    free(filter);

    return EXIT_SUCCESS;
}
