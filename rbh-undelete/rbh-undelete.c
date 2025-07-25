/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#include <dlfcn.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sysexits.h>
#include <getopt.h>

#include <robinhood.h>
#include <robinhood/config.h>
#include <robinhood/uri.h>
#include <robinhood/value.h>
#include <robinhood/filter.h>
#include <robinhood/filters/parser.h>

static struct rbh_backend *from, *to;

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

static void __attribute__((destructor))
destroy_to(void)
{
    const char *name;

    if (to) {
        name = to->name;
        rbh_backend_destroy(to);
        rbh_backend_plugin_destroy(name);
    }
}

static int
undelete_helper()
{
    const char *message =
        "Usage:"
        "  %s <URI> -arguments       General informations about rbh-undelete"
        " command\n"
        "  -h --help                 Show this message and exit\n"
        "  --list                    Display a list of deleted but archived "
        "entries\n"
        "  --restore                 Recreate a deleted entry that has been"
        "deleted and rebind it to its old content\n";
    return printf(message, program_invocation_short_name);
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
        if (context->helper_target)
            undelete_helper();
        else
            undelete_helper();

        exit(0);
    }
}

static void
rm_list(const char *prefix)
{
    const struct rbh_filter_options OPTIONS = { 0 };
    const struct rbh_filter_output OUTPUT = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = RBH_FP_NAMESPACE_XATTRS,
            .statx_mask = 0,
        },
    };
    const struct rbh_filter RM_TIME_FILTER = {
        .op = RBH_FOP_EXISTS,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAMESPACE_XATTRS,
                .xattr = "rm_time"
            },
            .value = {
                .type = RBH_VT_BOOLEAN,
                .boolean = true
            },
        },
    };
    struct rbh_mut_iterator *_fsentries;
    struct rbh_fsentry *fsentry;
    char regex[PATH_MAX];
    const char *rm_path;
    time_t rm_time;
    int rc;

    rc = snprintf(regex, sizeof(regex), "^%s", prefix);
    if (rc < 0 || rc >= sizeof(regex)) {
        fprintf(stderr, "Error while formatting regex\n");
        return;
    }

   const struct rbh_filter PATH_PREFIX_FILTER = {
        .op = RBH_FOP_REGEX,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAMESPACE_XATTRS,
                .xattr = "path"
            },
            .value = {
                .type = RBH_VT_REGEX,
                .regex = {
                    .string = regex,
                    .options = 0
                },
            },
        },
    };
   const struct rbh_filter *AND_FILTERS[] = {
        &RM_TIME_FILTER,
        &PATH_PREFIX_FILTER
    };
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_AND,
        .logical = {
            .filters = AND_FILTERS,
            .count = sizeof(AND_FILTERS) / sizeof(*AND_FILTERS)
        }
    };

    _fsentries = rbh_backend_filter(from, &FILTER, &OPTIONS, &OUTPUT);
    if (!_fsentries)
        return;

    printf("DELETED FILES:\n");
    while ((fsentry = rbh_mut_iter_next(_fsentries)) != NULL) {
        rm_path = fsentry_path(fsentry);
        rm_time = (time_t)fsentry_rm_time(fsentry);

        if (rm_path && rm_time != -1)
            printf("-- rm_path: %s   rm_time: %s \n", rm_path,
                   time_from_timestamp(&rm_time));
    }

    rbh_mut_iter_destroy(_fsentries);
}

static void
undelete(const char *path)
{
    const struct rbh_filter_projection ALL = {
        /* Archived entries that have been deleted no longer have a PARENT_ID
         * or a NAME stored inside the database
         **/
        .fsentry_mask = RBH_FP_ALL & ~RBH_FP_PARENT_ID & ~RBH_FP_NAME,
        .statx_mask = RBH_STATX_ALL & ~RBH_STATX_MNT_ID,
    };
    const char *last = strrchr(path, '/') ?: path;
    /*temporary pop of /mnt/lustre*/

    const struct rbh_filter PATH_FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAMESPACE_XATTRS,
                .xattr = "path"
            },
            .value = {
                .type = RBH_VT_STRING,
                .string = last
            },
        },
    };
    struct rbh_fsentry *fsentry;

    fsentry = rbh_backend_filter_one(from, &PATH_FILTER, &ALL);
    if (fsentry == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_filter_one");

    rbh_backend_undelete(to, fsentry, path);

    return;
}

int
main(int _argc, char *_argv[])
{
    struct command_context command_context = {0};
    struct rbh_raw_uri *raw_uri;
    char **option = NULL;
    struct rbh_uri *uri;
    int nb_cli_args;
    char **argv;
    int argc;

    if (_argc < 2)
        error(EX_USAGE, EINVAL,
              "invalid number of arguments, expected at least 1");

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

    from = rbh_backend_from_uri(argv[0], true);
    to = rbh_backend_from_uri(argv[1], true);

    raw_uri = rbh_raw_uri_from_string(argv[1]);
    if (raw_uri == NULL)
        error(EXIT_FAILURE, errno, "Cannot detect backend uri");

    uri = rbh_uri_from_raw_uri(raw_uri);
    if (uri == NULL)
        error(EXIT_FAILURE, errno, "Cannot detect given backend");
    free(raw_uri);

    option = malloc(sizeof(char *) *argc);
    if (option == NULL)
        error(EXIT_FAILURE, ENOMEM, "Failed to malloc 'option'");

    for (int i = 0 ; i < argc ; i++) {
        char *arg = argv[i];

        if (strcmp(arg, "--list") == 0)
            rm_list(uri->fsname);

        if (strcmp(arg, "--restore") == 0)
            undelete(uri->fsname);
    }

    free(option);
    free(uri);

    return EXIT_SUCCESS;
}
