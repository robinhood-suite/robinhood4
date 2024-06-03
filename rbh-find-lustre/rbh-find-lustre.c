/* This file is part of RobinHood 4
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
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

#include <rbh-find/actions.h>
#include <rbh-find/core.h>
#include <rbh-find/find_cb.h>

#include "filters.h"
#include "parser.h"
#include "actions.h"

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
        "usage: %s SOURCE [-h|--help] [PREDICATES] [ACTION]\n"
        "\n"
        "Query SOURCE's entries according to PREDICATE and do ACTION on each.\n"
        "This command supports all of rbh-find's predicates and actions, plus new\n"
        "ones specific to Lustre.\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE  a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    -h,--help             show this message and exit\n"
        "\n"
        "Predicate arguments:\n"
        "    -fid FID             filter entries based on their FID\n"
        "    -hsm-state {archived, dirty, exists, lost, noarchive, none, norelease, released}\n"
        "                         filter entries based of their HSM state.\n"
        "    -ost-index INDEX     filter entries based of the OST they are on.\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n"
        "Where:\n"
        "    BACKEND  is the name of a backend\n"
        "    FSNAME   the name of the backend instance (a path for a\n"
        "             filesystem, a database name for a database)\n"
        "    PATH/ID  is the path/id of an fsentry managed by BACKEND:FSNAME\n"
        "             (ID must be enclosed in square brackets '[ID]' to distinguish it\n"
        "             from a path)\n";

    return printf(message, program_invocation_short_name);
}

enum command_line_token
lustre_predicate_or_action(const char *string)
{
    switch (string[0]) {
    case '-':
        switch (string[1]) {
        case 'e':
            if (strncmp(&string[2], "xpired", strlen("xpired")) != 0)
                break;

            if (string[strlen("-expired")] == 0)
                return CLT_PREDICATE;

            if (strcmp(&string[strlen("-expired")], "-at") == 0)
                return CLT_PREDICATE;
            break;
        case 'f':
            if (!strcmp(&string[2], "id"))
                return CLT_PREDICATE;
            break;
        case 'h':
            if (!strcmp(&string[2], "sm-state"))
                return CLT_PREDICATE;
            break;
        case 'l':
            if (!strcmp(&string[2], "ayout-pattern"))
                return CLT_PREDICATE;
            break;
        case 'o':
            if (!strcmp(&string[2], "st"))
                return CLT_PREDICATE;
            break;
        case 's':
            if (!strcmp(&string[2], "tripe-count"))
                return CLT_PREDICATE;
            else if (!strcmp(&string[2], "tripe-size"))
                return CLT_PREDICATE;
            break;
        }
        break;
    }

    return find_predicate_or_action(string);
}

static bool
predicate_has_argument(int predicate)
{
    return predicate != LPRED_EXPIRED;
}

static struct rbh_filter *
lustre_parse_predicate(struct find_context *ctx, int *arg_idx)
{
    struct rbh_filter *filter;
    int i = *arg_idx;
    int predicate;

    predicate = str2lustre_predicate(ctx->argv[i]);

    if (predicate_has_argument(predicate) && i + 1 >= ctx->argc)
        error(EX_USAGE, 0, "missing argument to `%s'", ctx->argv[i]);

    /* In the following block, functions should call error() themselves rather
     * than returning.
     *
     * Errors are most likely fatal (not recoverable), and this allows for
     * precise and meaningul error messages.
     */
    switch (predicate) {
    case LPRED_EXPIRED:
        filter = expired2filter();
        break;
    case LPRED_EXPIRED_AT:
        filter = expired_at2filter(ctx->argv[++i]);
        break;
    case LPRED_FID:
        filter = fid2filter(ctx->argv[++i]);
        break;
    case LPRED_HSM_STATE:
        filter = hsm_state2filter(ctx->argv[++i]);
        break;
    case LPRED_OST_INDEX:
        filter = ost_index2filter(ctx->argv[++i]);
        break;
    case LPRED_STRIPE_COUNT:
        filter = stripe_count2filter(ctx->argv[++i]);
        break;
    case LPRED_STRIPE_SIZE:
        filter = stripe_size2filter(ctx->argv[++i]);
        break;
    case LPRED_LAYOUT_PATTERN:
        filter = layout_pattern2filter(ctx->argv[++i]);
        break;
    default:
        filter = find_parse_predicate(ctx, &i);
        break;
    }
    assert(filter != NULL);

    *arg_idx = i;
    return filter;
}

static void
handle_config_option(int argc, char *argv[], int index)
{
    if (index + 1 >= argc)
        error(EX_USAGE, EINVAL, "'--config' option requires a file");

    if (rbh_config_open(argv[index + 1]))
        error(EX_USAGE, errno,
              "Failed to open configuration file '%s'", argv[i + 1]);
}

static int
check_command_options(int argc, char *argv[])
{
    int rc;
    int i;

    for (i = 0; i < argc; i++) {
        if (*argv[i] != '-')
            return i;

        if (strcmp(argv[i], "-c") == 0 && strcmp(argv[i], "--config") == 0) {
            handle_config_option(argc, argv, i);
            i++;
        }

        if (strcmp(argv[i], "-h") == 0 && strcmp(argv[i], "--help") == 0) {
            usage();
            exit(0);
        }
    }

    return i;
}

int
main(int _argc, char *_argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "help",
            .val = 'h',
        },
        {}
    };
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

    checked_options = check_command_options(argc, argv);

    ctx.argc = argc - checked_options;
    ctx.argv = &argv[checked_options];

    ctx.pre_action_callback = &find_pre_action;
    ctx.exec_action_callback = &find_exec_action;
    ctx.post_action_callback = &find_post_action;
    ctx.parse_predicate_callback = &lustre_parse_predicate;
    ctx.pred_or_action_callback = &lustre_predicate_or_action;
    ctx.print_directive = &fsentry_print_lustre_directive;

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
    filter = parse_expression(&ctx, &index, NULL, &sorts, &sorts_count);
    if (index != ctx.argc)
        error(EX_USAGE, 0, "you have too many ')'");

    if (!ctx.action_done)
        find(&ctx, ACT_PRINT, &index, filter, sorts, sorts_count);
    free(filter);

    rbh_config_free();

    return EXIT_SUCCESS;
}
