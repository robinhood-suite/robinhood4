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
#include "robinhood/backends/lustre.h"
#include <robinhood/backends/posix_extension.h>
#include <robinhood/config.h>
#include <robinhood/uri.h>
#include <robinhood/value.h>

enum rbh_undelete_option {
    RBH_UNDELETE_RESTORE = 1 << 0,
    RBH_UNDELETE_LIST    = 1 << 1,
    RBH_UNDELETE_OUTPUT  = 1 << 2,
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

struct rbh_fsentry *
get_fsentry_from_metadata_source_with_path(const char *path)
{
    const struct rbh_filter_projection ALL = {
        /**
         * Archived entries that have been deleted no longer have a PARENT_ID
         * or a NAME stored inside the database
         */
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
        error(EXIT_FAILURE, ENOENT, "Failed to find '%s' in source URI",
              path);

    if (!(fsentry->mask & RBH_FP_STATX))
        error(EXIT_FAILURE, ENOENT,
              "Entry '%s' in source URI is missing statx information",
              path);

    return fsentry;
}

struct rbh_fsentry *
get_fsentry_from_metadata_source_with_fid(const struct rbh_value *fid_value)
{
    const struct rbh_filter_projection ALL = {
        .fsentry_mask = RBH_FP_ALL,
        .statx_mask = RBH_STATX_ALL,
    };
    const struct rbh_filter FID_FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID,
            },
            .value = *fid_value,
        },
    };

    return rbh_backend_filter_one(metadata_source, &FID_FILTER, &ALL);
}

/*----------------------------------------------------------------------------*
 |                                undelete()                                  |
 *----------------------------------------------------------------------------*/

static void
undelete(const char *output)
{
    struct rbh_fsevent delete_event = { .type = RBH_FET_DELETE };
    struct rbh_iterator *delete_iter;
    struct rbh_fsentry *new_fsentry;
    struct rbh_fsentry *fsentry;
    int save_errno;
    int rc;

    fsentry = get_fsentry_from_metadata_source_with_path(
        relative_undelete_target_path
    );

    delete_event.id = fsentry->id;

    new_fsentry = rbh_backend_undelete(target_entry,
                                       output != NULL ?
                                           output : full_undelete_target_path,
                                       fsentry);
    if (new_fsentry == NULL)
        error(EXIT_FAILURE, ENOENT, "Failed to undelete '%s'",
              full_undelete_target_path);

    delete_iter = rbh_iter_array(&delete_event, sizeof(delete_event), 1, NULL);
    if (delete_iter == NULL)
        error(EXIT_FAILURE, errno, "rbh_iter_array");

    rc = rbh_backend_update(metadata_source, delete_iter);
    save_errno = errno;
    rbh_iter_destroy(delete_iter);
    if (rc < 0)
        error(EXIT_FAILURE, save_errno, "rbh_backend_update (DELETE)");
}

static void
set_targets(struct rbh_uri *target_uri, const char *mountpoint)
{
    size_t mountpoint_len;

    mountpoint_len = strlen(mountpoint);

    if (target_uri->fsname[0] != '/') {
        char full_path[PATH_MAX];

        if (getcwd(full_path, sizeof(full_path)) == NULL)
            error(EXIT_FAILURE, errno, "getcwd");

        if (asprintf(&full_undelete_target_path, "%s/%s",
                     full_path, target_uri->fsname) == -1)
            error(EXIT_FAILURE, errno, "Failed create full target path");
    } else {
        full_undelete_target_path = strdup(target_uri->fsname);
    }

    if (full_undelete_target_path == NULL)
        error(EXIT_FAILURE, errno, "Failed to duplicate target name");

    if (strncmp(full_undelete_target_path, mountpoint, mountpoint_len) != 0 ||
        full_undelete_target_path[mountpoint_len] == '\0')
        error(EXIT_FAILURE, ENOTSUP,
              "Mountpoint recorded '%s' in the source URI isn't in the path to undelete '%s'",
              mountpoint, full_undelete_target_path);

    relative_undelete_target_path = &full_undelete_target_path[mountpoint_len];
}

static const char *
get_mountpoint_from_source()
{
    struct rbh_value_map *mountpoint_value_map;

    mountpoint_value_map = rbh_backend_get_info(metadata_source,
                                                RBH_INFO_MOUNTPOINT);
    if (mountpoint_value_map == NULL || mountpoint_value_map->count != 1) {
        fprintf(stderr, "Failed to get mountpoint from source URI\n");
        return NULL;
    }

    return mountpoint_value_map->pairs[0].value->string;
}

static const char *
get_mountpoint_from_current_system()
{
    struct rbh_value_pair *pwd_pair = NULL;
    const struct rbh_value *fsentry_path;
    struct rbh_value *pwd_value = NULL;
    struct rbh_posix_enrich_ctx ctx;
    struct rbh_fsentry *fsentry;
    struct rbh_value_pair pair;
    ssize_t pwd_pair_count = 1;
    char full_path[PATH_MAX];
    char *mountpoint;
    char *substr;
    int rc;

    pwd_value = malloc(sizeof(*pwd_value));
    if (pwd_value == NULL)
        error(EXIT_FAILURE, errno, "Failed to allocate pwd_value");

    if (getcwd(full_path, sizeof(full_path)) == NULL)
        error(EXIT_FAILURE, errno, "getcwd");

    pwd_value->type = RBH_VT_STRING;
    pwd_value->string = full_path;

    pwd_pair = malloc(sizeof(*pwd_pair));
    if (pwd_pair == NULL)
        error(EXIT_FAILURE, errno, "Failed to allocate pwd_pair");

    pwd_pair->key = "path";
    pwd_pair->value = pwd_value;

    ctx.einfo.inode_xattrs = pwd_pair;
    ctx.einfo.inode_xattrs_count = &pwd_pair_count;

    rc = rbh_backend_get_attribute(target_entry, RBH_LEF_LUSTRE | RBH_LEF_FID,
                                   &ctx, &pair, 1);
    free(pwd_value);
    free(pwd_pair);
    if (rc == -1) {
        fprintf(stderr, "Failed to get FID of current path '%s'\n", full_path);
        return NULL;
    }

    fsentry = get_fsentry_from_metadata_source_with_fid(pair.value);
    if (fsentry == NULL) {
        /* XXX: this log may appear a lot, but isn't fatal if there is a
         * mountpoint recorded in the source URI. Should we keep it?
         */
        //fprintf(stderr,
        //        "Failed to find fsentry associated with current path\n");
        return NULL;
    }

    fsentry_path = rbh_fsentry_find_ns_xattr(fsentry, "path");
    if (fsentry_path == NULL) {
        fprintf(stderr, "Cannot get path of '%s' in source URI\n", full_path);
        return NULL;
    }

    substr = strstr(full_path, fsentry_path->string);
    if (substr == NULL) {
        fprintf(stderr,
                "PWD fetched from the database ('%s') is not part of current PWD '%s'\n",
                fsentry_path->string, full_path);
        return NULL;
    }

    *substr = '\0';
    mountpoint = strdup(full_path);
    if (mountpoint == NULL)
        error(EXIT_FAILURE, errno, "Failed to allocate mountpoint");

    return mountpoint;
}

static const char *
get_mountpoint()
{
    const char *mountpoint = get_mountpoint_from_current_system();

    return mountpoint ? mountpoint : get_mountpoint_from_source();
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
    struct rbh_raw_uri *raw_uri;
    const char *output = NULL;
    const char *mountpoint;
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

    metadata_source = rbh_backend_from_uri(argv[optind++], true);

    raw_uri = rbh_raw_uri_from_string(argv[optind++]);
    if (raw_uri == NULL)
        error(EXIT_FAILURE, errno, "Cannot detect backend uri");

    uri = rbh_uri_from_raw_uri(raw_uri);
    free(raw_uri);
    if (uri == NULL)
        error(EXIT_FAILURE, errno, "Cannot detect given backend");

    target_entry = rbh_backend_and_branch_from_uri(uri, false);

    mountpoint = get_mountpoint();
    if (mountpoint == NULL)
        error(EXIT_FAILURE, ENOENT,
              "Failed to retrieve mountpoint to undelete '%s'",
              uri->fsname);

    set_targets(uri, mountpoint);
    free(uri);

    if (flags & RBH_UNDELETE_RESTORE)
        undelete(output);

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

    return EXIT_SUCCESS;
}
