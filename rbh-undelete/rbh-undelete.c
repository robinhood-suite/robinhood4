/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#include <linux/limits.h>
#include <sysexits.h>

#include <robinhood.h>
#include <robinhood/config.h>
#include <robinhood/uri.h>
#include <robinhood/value.h>
#include <robinhood/filter.h>
#include <robinhood/filters/parser.h>

enum rbh_undelete_option {
    RBH_UNDELETE_RESTORE = 1 << 0,
    RBH_UNDELETE_LIST    = 1 << 1
};

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
         */
        .fsentry_mask = RBH_FP_ALL & ~RBH_FP_PARENT_ID & ~RBH_FP_NAME,
        .statx_mask = RBH_STATX_ALL,
    };
    const char *actual_path;

    /* Temporary pop of '/mnt/lustre' */
    assert(strlen(path) >= 11);  // 11 = strlen("/mnt/lustre")
    actual_path = path + 11;

    const struct rbh_filter PATH_FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAMESPACE_XATTRS,
                .xattr = "path"
            },
            .value = {
                .type = RBH_VT_STRING,
                .string = actual_path
            },
        },
    };
    struct rbh_fsevent delete_event = { .type = RBH_FET_DELETE };
    struct rbh_iterator *delete_iter;
    struct rbh_fsentry *new_fsentry;
    struct rbh_fsentry *fsentry;

    fsentry = rbh_backend_filter_one(metadata_source, &PATH_FILTER, &ALL);
    if (fsentry == NULL) {
        fprintf(stderr, "Failed to find '%s' in source URI",
                undelete_target_path);
        return ENOENT;
    }

    new_fsentry = rbh_backend_undelete(target_entry, path, fsentry);
    if (new_fsentry == NULL) {
        fprintf(stderr, "Error while returning fsentry from undelete\n");
        return -1;
    }

    delete_event.id = fsentry->id;

    delete_iter = rbh_iter_array(&delete_event, sizeof(delete_event), 1);
    if (delete_iter == NULL)
        error(EXIT_FAILURE, errno, "rbh_iter_array");

    if (rbh_backend_update(metadata_source, delete_iter) < 0) {
        int save_errno = errno;

        rbh_iter_destroy(delete_iter);
        error(EXIT_FAILURE, save_errno, "rbh_backend_update (DELETE)");
    }

    rbh_iter_destroy(delete_iter);

    return 0;
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
    if (undelete_target_path == NULL) {
        free(uri);
        error(EXIT_FAILURE, errno, "Failed to duplicate target name");
    }

    target_entry = rbh_backend_and_branch_from_uri(uri, false);
    free(uri);
}

/*----------------------------------------------------------------------------*
 |                                  rm_list()                                 |
 *----------------------------------------------------------------------------*/

static void
rm_list(const char *regex)
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
    struct rbh_mut_iterator *_fsentries;
    struct rbh_fsentry *fsentry;

    _fsentries = rbh_backend_filter(metadata_source, &FILTER, &OPTIONS,
                                    &OUTPUT);
    if (!_fsentries)
        return;

    printf("DELETED FILES:\n");
    while ((fsentry = rbh_mut_iter_next(_fsentries)) != NULL) {
        const struct rbh_value *ns_rm_time;
        const struct rbh_value *ns_rm_path;
        const char *rm_path;
        time_t rm_time;

        ns_rm_path = rbh_fsentry_find_ns_xattr(fsentry, "path");
        if (ns_rm_path == NULL) {
            fprintf(stderr, "'%s' is archived and deleted but has no rm_path\n",
                    fsentry->name);
            continue;
        }

        rm_path = ns_rm_path->string;

        ns_rm_time = rbh_fsentry_find_ns_xattr(fsentry, "rm_time");
        if (ns_rm_time == NULL) {
            fprintf(stderr, "'%s' is archived and deleted but has no rm_time\n",
                    fsentry->name);
            continue;
        }

        rm_time = (time_t) ns_rm_time->int64;

        printf("-- rm_path: %s   rm_time: %s \n", rm_path,
               time_from_timestamp(&rm_time));
    }

    rbh_mut_iter_destroy(_fsentries);
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
        "    -l,--list            Display a list of deleted but archived\n"
        "                         entries\n"
        "    -r,--restore         Recreate a deleted entry that has been\n"
        "                         deleted and rebind it to its old content\n"
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
    int flags = 0;
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

    for (int i = 0 ; i < argc ; i++) {
        char *arg = argv[i];

        if (strcmp(arg, "--list") == 0 || strcmp(arg, "-l") == 0)
            flags |= RBH_UNDELETE_LIST;

        if (strcmp(arg, "--restore") == 0 || strcmp(arg, "-r") == 0)
            flags |= RBH_UNDELETE_RESTORE;
    }

    if (flags & RBH_UNDELETE_RESTORE)
        return undelete(undelete_target_path);

    if (flags & RBH_UNDELETE_LIST) {
        char regex[PATH_MAX];

        if (snprintf(regex, sizeof(regex), "^%s", undelete_target_path) == -1) {
            fprintf(stderr,
                    "Error while formatting regex associated with '%s'\n",
                    undelete_target_path);
            return EXIT_FAILURE;
        }

        rm_list(regex);
    }

    return 0;
}
