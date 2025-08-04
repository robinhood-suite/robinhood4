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
static char *full_undelete_target_path, *relative_undelete_target_path;

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

    /* `relative_undelete_target_path` is a substring of
     * `full_undelete_target_path`, no need to free it.
     */
    if (full_undelete_target_path)
        free(full_undelete_target_path);
}

/*----------------------------------------------------------------------------*
 |                              build_paths()                                 |
 *----------------------------------------------------------------------------*/

static char *
ensure_slash(const char* path)
{
    char *buf;

    if (path[0] == '/')
        return strdup(path);
    else {
        if (asprintf(&buf, "/%s", path) < 0)
            return NULL;
        return buf;
    }
}

int
build_paths(const char *path, const char *mountpoint, char **_relative_path,
            char **_full_path)
{
    char *relative_path = NULL;
    char *full_path = NULL;
    size_t mpt_len;

    mpt_len = strlen(mountpoint);

    if (strncmp(path, mountpoint, mpt_len) == 0 && path[mpt_len] == '/') {
        relative_path = strdup(path + mpt_len);
        if (!relative_path)
            return -1;
        full_path = strdup(path);
        if (!full_path) {
            free(relative_path);
            return -1;
        }
    } else {
        relative_path = ensure_slash(path);
        if (!relative_path)
            return -1;

        if (asprintf(&full_path, "%s%s", mountpoint, relative_path) < 0) {
            free(relative_path);
            return -1;
        }
    }

    *_relative_path = relative_path;
    *_full_path = full_path;

    return 0;
}

/*----------------------------------------------------------------------------*
 |                                undelete()                                  |
 *----------------------------------------------------------------------------*/

#define FREE(x, y) do { if ((x) != NULL) { free(x); (x) = NULL; } \
    if ((y) != NULL) { free(y); (y) = NULL; } } while (0)

static void
undelete()
{
    const struct rbh_filter_projection ALL = {
        /**
         * Archived entries that have been deleted no longer have a PARENT_ID
         * or a NAME stored inside the database
         */
        .fsentry_mask = RBH_FP_ALL & ~RBH_FP_PARENT_ID & ~RBH_FP_NAME,
        .statx_mask = RBH_STATX_ALL,
    };
    struct rbh_fsevent delete_event = { .type = RBH_FET_DELETE };
    struct rbh_iterator *delete_iter;
    struct rbh_fsentry *new_fsentry;
    struct rbh_fsentry *fsentry;
    const struct rbh_filter PATH_FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAMESPACE_XATTRS,
                .xattr = "path"
            },
            .value = {
                .type = RBH_VT_STRING,
                .string = relative_undelete_target_path
            },
        },
    };

    fsentry = rbh_backend_filter_one(metadata_source, &PATH_FILTER, &ALL);
    if (fsentry == NULL)
        error(EXIT_FAILURE, ENOENT, "Failed to find '%s' in source URI",
              relative_undelete_target_path);

    new_fsentry = rbh_backend_undelete(target_entry, full_undelete_target_path,
                                       fsentry);
    if (new_fsentry == NULL)
        error(EXIT_FAILURE, ENOENT, "Failed to undelete '%s'",
              full_undelete_target_path);

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
}

static void
set_targets(const char *target_uri, const char *mountpoint)
{
    struct rbh_raw_uri *raw_uri;
    size_t mountpoint_len;
    struct rbh_uri *uri;

    raw_uri = rbh_raw_uri_from_string(target_uri);
    if (raw_uri == NULL)
        error(EXIT_FAILURE, errno, "Cannot detect backend uri");

    uri = rbh_uri_from_raw_uri(raw_uri);
    free(raw_uri);
    if (uri == NULL)
        error(EXIT_FAILURE, errno, "Cannot detect given backend");

    if (uri->fsname[0] != '/')
        error(EXIT_FAILURE, ENOTSUP, "Cannot undelete relative path");

    full_undelete_target_path = strdup(uri->fsname);
    if (full_undelete_target_path == NULL)
        error(EXIT_FAILURE, errno, "Failed to duplicate target name");

    mountpoint_len = strlen(mountpoint);

    if (strncmp(full_undelete_target_path, mountpoint, mountpoint_len) != 0 ||
        full_undelete_target_path[mountpoint_len] == '\0')
        error(EXIT_FAILURE, ENOTSUP,
              "Mountpoint recorded '%s' in the source URI isn't in the path to undelete '%s'",
              mountpoint, full_undelete_target_path);

    relative_undelete_target_path = &full_undelete_target_path[mountpoint_len];

    target_entry = rbh_backend_and_branch_from_uri(uri, false);
    free(uri);
}

static const char *
get_source_mountpoint()
{
    struct rbh_value_map *mountpoint_value_map;

    mountpoint_value_map = rbh_backend_get_info(metadata_source,
                                                RBH_INFO_MOUNTPOINT);
    if (mountpoint_value_map == NULL || mountpoint_value_map->count != 1)
        fprintf(stderr, "Failed to get mountpoint from source URI\n");

    return mountpoint_value_map->pairs[0].value->string;
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
        error(EXIT_FAILURE, errno,
              "Failed to get undeletable entries in '%s'",
              relative_undelete_target_path);

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
    const char *mountpoint;
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
    mountpoint = get_source_mountpoint();
    set_targets(argv[1], mountpoint);

    for (int i = 0 ; i < argc ; i++) {
        char *arg = argv[i];

        if (strcmp(arg, "--list") == 0 || strcmp(arg, "-l") == 0)
            flags |= RBH_UNDELETE_LIST;

        if (strcmp(arg, "--restore") == 0 || strcmp(arg, "-r") == 0)
            flags |= RBH_UNDELETE_RESTORE;
    }

    if (flags & RBH_UNDELETE_RESTORE)
        undelete();

    if (flags & RBH_UNDELETE_LIST) {
        char regex[PATH_MAX];

        if (snprintf(regex, sizeof(regex), "^%s",
                     relative_undelete_target_path) == -1) {
            fprintf(stderr,
                    "Error while formatting regex associated with '%s'\n",
                    relative_undelete_target_path);
            return EXIT_FAILURE;
        }

        rm_list(regex);
    }

    return 0;
}
