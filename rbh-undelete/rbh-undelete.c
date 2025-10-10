/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#include <getopt.h>
#include <linux/limits.h>
#include <sysexits.h>
#include <unistd.h>

#include <robinhood.h>
#include <robinhood/alias.h>
#include <robinhood/config.h>
#include <robinhood/uri.h>
#include <robinhood/value.h>

#include "undelete.h"

enum rbh_undelete_option {
    RBH_UNDELETE_RESTORE = 1 << 0,
    RBH_UNDELETE_LIST    = 1 << 1,
    RBH_UNDELETE_OUTPUT  = 1 << 2,
};

static void
clean_undelete_context(struct undelete_context *context)
{
    const char *name;

    if (context->source) {
        name = context->source->name;
        rbh_backend_destroy(context->source);
        rbh_backend_plugin_destroy(name);
    }

    if (context->target) {
        name = context->target->name;
        rbh_backend_destroy(context->target);
        rbh_backend_plugin_destroy(name);
    }

    free(context->mountpoint);
    free(context->absolute_target_path);
}

static struct rbh_uri *
get_rbh_uri_from_string(const char *arg_uri)
{
    struct rbh_raw_uri *raw_uri;
    struct rbh_uri *uri;

    raw_uri = rbh_raw_uri_from_string(arg_uri);
    if (raw_uri == NULL) {
        fprintf(stderr, "Cannot detect backend URI '%s': %s (%d)\n",
                arg_uri, strerror(errno), errno);
        return NULL;
    }

    uri = rbh_uri_from_raw_uri(raw_uri);
    free(raw_uri);
    if (uri == NULL) {
        fprintf(stderr, "Cannot detect given backend '%s': %s (%d)\n",
                arg_uri, strerror(errno), errno);
        return NULL;
    }

    return uri;
}

/*----------------------------------------------------------------------------*
 |                                    cli                                     |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                              usage()                               |
     *--------------------------------------------------------------------*/

static int
usage()
{
    const char *message =
        "Usage: %s [OPTIONS] SOURCE DEST\n"
        "\n"
        "Undelete DEST's entry using SOURCES's metadata\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE   a robinhood URI\n"
        "    DEST     a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    -c,--config PATH     The configuration file to use\n"
        "    -h,--help            Show this message and exit\n"
        "    -l,--list            Display a list of deleted but archived\n"
        "                         entries\n"
        "    --output OUTPUT      The path where the file will be recreated\n"
        "    -r,--restore         Recreate a deleted entry that has been\n"
        "                         deleted and rebind it to its old content\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n\n";

    return printf(message, program_invocation_short_name);
}

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "config",
            .has_arg = required_argument,
            .val = 'c',
        },
        {
            .name = "help",
            .has_arg = no_argument,
            .val = 'h',
        },
        {
            .name = "list",
            .has_arg = no_argument,
            .val = 'l',
        },
        {
            .name = "output",
            .has_arg = required_argument,
            .val = 'o',
        },
        {
            .name = "restore",
            .has_arg = no_argument,
            .val = 'r',
        },
        {}
    };
    struct undelete_context context = { 0 };
    const char *output = NULL;
    struct rbh_uri *uri;
    int flags = 0;
    char c;
    int rc;

    rc = rbh_config_from_args(argc - 1, argv + 1);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to load configuration file");

    rbh_apply_aliases(&argc, &argv);

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "c:hlo:r", LONG_OPTIONS,
                            NULL)) != -1) {
        switch (c) {
        case 'c':
            /* already parsed */
            break;
        case 'h':
            usage();
            return 0;
        case 'l':
            flags |= RBH_UNDELETE_LIST;
            break;
        case 'o':
            output = strdup(optarg);
            if (output == NULL)
                error(EXIT_FAILURE, ENOMEM, "strdup");

            flags |= RBH_UNDELETE_OUTPUT;
            break;
        case 'r':
            flags |= RBH_UNDELETE_RESTORE;
            break;
        default:
            /* getopt_long() prints meaningful error messages itself */
            exit(EX_USAGE);
        }
    }

    if (argc - optind < 2)
        error(EX_USAGE, 0, "not enough arguments");
    if (argc - optind > 2)
        error(EX_USAGE, 0, "too many arguments");

    if (flags & RBH_UNDELETE_LIST && flags & RBH_UNDELETE_RESTORE)
        error(EX_USAGE, ENOMEM,
              "cannot list and restore a file at the same time");

    if (flags & RBH_UNDELETE_OUTPUT && (flags & RBH_UNDELETE_RESTORE) == 0)
        error(EX_USAGE, ENOMEM,
              "output option can only be used with the restore option");

    context.source = rbh_backend_from_uri(argv[optind++], true);

    uri = get_rbh_uri_from_string(argv[optind++]);
    if (uri == NULL)
        goto clean_context;

    context.target = rbh_backend_and_branch_from_uri(uri, false);

    context.mountpoint = get_mountpoint(&context);
    if (context.mountpoint == NULL) {
        rc = ENOENT;
        goto clean_context;
    }

    rc = set_targets(uri->fsname, &context);
    free(uri);
    if (rc)
        goto clean_context;

    if (flags & RBH_UNDELETE_RESTORE)
        rc = undelete(&context, output);
    else if (flags & RBH_UNDELETE_LIST)
        rc = list_deleted_entries(&context);

clean_context:
    clean_undelete_context(&context);

    return rc;
}
