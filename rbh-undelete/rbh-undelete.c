/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#include <sysexits.h>

#include <robinhood.h>
#include <robinhood/config.h>
#include <robinhood/uri.h>
#include <robinhood/value.h>
#include <robinhood/filter.h>
#include <robinhood/filters/parser.h>

static struct rbh_backend *metadata_source, *target_entry;
static char *undelete_target_path;

static void __attribute__((destructor))
destroy_global_variables(void)
{
    const char *name;

    if (metadata_source) {
        name = metadata_source->name;
        rbh_backend_destroy(metadata_source);
        rbh_backend_plugin_destroy(name);
    }

    if (target_entry) {
        name = target_entry->name;
        rbh_backend_destroy(target_entry);
        rbh_backend_plugin_destroy(name);
    }

    if (undelete_target_path)
        free(undelete_target_path);
}

/*----------------------------------------------------------------------------*
 |                                undelete()                                  |
 *----------------------------------------------------------------------------*/

static int
undelete(const char *path)
{
    const struct rbh_filter_projection ALL = {
        /* Archived entries that have been deleted no longer have a PARENT_ID
         * or a NAME stored inside the database
         * */
        .fsentry_mask = RBH_FP_ALL & ~RBH_FP_PARENT_ID & ~RBH_FP_NAME,
        .statx_mask = RBH_STATX_ALL,
    };
    const struct rbh_filter PATH_FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAMESPACE_XATTRS,
                .xattr = "path"
            },
            .value = {
                .type = RBH_VT_STRING,
                .string = path
            },
        },
    };
    struct rbh_fsentry *fsentry;

    fsentry = rbh_backend_filter_one(metadata_source, &PATH_FILTER, &ALL);
    if (fsentry == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_filter_one");

    return EXIT_SUCCESS;
}

static void
set_targets(const char *target_uri)
{
    struct rbh_raw_uri *raw_uri;
    struct rbh_uri *uri;

    raw_uri = rbh_raw_uri_from_string(target_uri);
    if (raw_uri == NULL)
        error(EXIT_FAILURE, errno, "Cannot detect backend uri");

    uri = rbh_uri_from_raw_uri(raw_uri);
    free(raw_uri);
    if (uri == NULL)
        error(EXIT_FAILURE, errno, "Cannot detect given backend");

    undelete_target_path = strdup(uri->fsname);
    if (undelete_target_path == NULL)
        error(EXIT_FAILURE, errno, "Failed to duplicate target name");

    target_entry = rbh_backend_and_branch_from_uri(uri, false);
    free(uri);
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
        "Usage: %s [-h|--help] SOURCE DEST\n"
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
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n\n";

    return printf(message, program_invocation_short_name);
}

static void
get_command_options(int argc, char *argv[], struct command_context *context)
{
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            context->helper = true;

        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc)
                error(EXIT_FAILURE, EINVAL,
                      "missing configuration file value");

            context->config_file = argv[i + 1];
        }
    }
}

static void
apply_command_options(struct command_context *context, int argc, char *argv[])
{
    if (context->helper) {
        usage();
        exit(0);
    }
}

int
main(int _argc, char *_argv[])
{
    struct command_context command_context = {0};
    int nb_cli_args;
    char **argv;
    int argc;

    if (_argc < 2)
        error(EX_USAGE, EINVAL,
              "invalid number of arguments, expected at least 2");

    argc = _argc - 1;
    argv = &_argv[1];

    nb_cli_args = rbh_count_args_before_uri(argc, argv);
    get_command_options(nb_cli_args, argv, &command_context);

    rbh_config_load_from_path(command_context.config_file);

    nb_cli_args = rbh_count_args_before_uri(argc, argv);
    get_command_options(nb_cli_args, argv, &command_context);
    apply_command_options(&command_context, argc, argv);

    argc = argc - nb_cli_args;
    argv = &argv[nb_cli_args];

    metadata_source = rbh_backend_from_uri(argv[0], true);
    set_targets(argv[1]);

    return undelete(undelete_target_path);
}
