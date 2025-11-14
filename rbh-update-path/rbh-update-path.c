/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <error.h>
#include <getopt.h>
#include <sysexits.h>

#include <robinhood.h>

static struct rbh_backend *backend;

static void __attribute__((destructor))
destroy_backend(void)
{
    const char *name;

    if (backend) {
        name = backend->name;
        rbh_backend_destroy(backend);
        rbh_backend_plugin_destroy(name);
    }
}

static struct rbh_mut_iterator *
_get_entries(struct rbh_filter *filter)
{
    const struct rbh_filter_projection proj = {
        .fsentry_mask = RBH_FP_ID | RBH_FP_PARENT_ID | RBH_FP_NAME |
                        RBH_FP_NAMESPACE_XATTRS | RBH_FP_STATX,
        .statx_mask = RBH_STATX_TYPE,
    };
    const struct rbh_filter_options option = {0};
    const struct rbh_filter_output output = {
        .type = RBH_FOT_PROJECTION,
        .projection = proj,
    };
    struct rbh_mut_iterator *fsentries;

    fsentries = rbh_backend_filter(backend, filter, &option, &output, NULL);
    if (fsentries == NULL) {
        if (errno == RBH_BACKEND_ERROR)
            error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
        else
            error(EXIT_FAILURE, errno,
                  "failed to execute filter on backend '%s'", backend->name);
    }

    free(filter);

    return fsentries;
}

static struct rbh_mut_iterator *
get_entry_children(struct rbh_fsentry *entry)
{
    const struct rbh_filter_field *field;
    struct rbh_filter *filter;

    field = str2filter_field("parent-id");
    filter = rbh_filter_compare_binary_new(RBH_FOP_EQUAL, field,
                                           entry->id.data, entry->id.size);
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "failed to create filter");

    return _get_entries(filter);
}

static struct rbh_mut_iterator *
get_entry_without_path()
{
    const struct rbh_filter_field *field = str2filter_field("ns-xattrs.path");
    struct rbh_filter *filter_path;
    struct rbh_filter *filter;

    filter_path = rbh_filter_exists_new(field);
    if (filter_path == NULL)
        error(EXIT_FAILURE, errno, "failed to create path filter");

    filter = rbh_filter_not(filter_path);

    return _get_entries(filter);
}

static struct rbh_fsevent *
generate_fsevent_ns_xattrs(struct rbh_fsentry *entry, struct rbh_value *value)
{
    struct rbh_value_map xattrs;
    struct rbh_fsevent *fsevent;
    struct rbh_value_pair pair;

    pair.key = "path";
    pair.value = value;

    xattrs.pairs = &pair;
    xattrs.count = 1;

    fsevent = rbh_fsevent_ns_xattr_new(&entry->id, &xattrs, &entry->parent_id,
                                       entry->name);

    if (fsevent == NULL)
        error(EXIT_FAILURE, errno, "failed to generate fsevent");

    return fsevent;
}

static void
update_path()
{
    struct rbh_mut_iterator *fsentries;
    struct rbh_mut_iterator *children;
    struct rbh_iterator *update_iter;
    struct rbh_fsevent *fsevent;
    int rc;

    fsentries = get_entry_without_path();

    while (true) {
        struct rbh_fsentry *entry = rbh_mut_iter_next(fsentries);

        if (entry == NULL) {
            if (errno == ENODATA)
                break;

            if (errno == RBH_BACKEND_ERROR)
                error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
            else
                error(EXIT_FAILURE, errno, "failed to retrieve entry");
        }

        printf("parent: %s\n", entry->name);

        /* If it's not a directory, no need to update its children */
        if (!S_ISDIR(entry->statx->stx_mode)) {
            free(entry);
            continue;
        }

        children = get_entry_children(entry);

        while (true) {
            struct rbh_fsentry *child = rbh_mut_iter_next(children);

            if (child == NULL) {
                if (errno == ENODATA)
                    break;

                if (errno == RBH_BACKEND_ERROR)
                    error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
                else
                    error(EXIT_FAILURE, errno, "failed to retrieve child");
            }

            printf("child: %s\n", child->name);

            /* TODO: store all the fsevent in a list and call rbh_backend_update
             * one time
             */
            fsevent = generate_fsevent_ns_xattrs(child, NULL);
            update_iter = rbh_iter_array(fsevent, sizeof(*fsevent), 1, NULL);

            rc = rbh_backend_update(backend, update_iter);
            if (rc == -1)
                error(EXIT_FAILURE, errno, "failed to update '%s'",
                      child->name);

            rbh_iter_destroy(update_iter);
            free(fsevent);
            free(child);
        }

        rbh_mut_iter_destroy(children);
        free(entry);
    }

    rbh_mut_iter_destroy(fsentries);
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
        {}
    };
    int rc;
    char c;

    rc = rbh_config_from_args(argc - 1, argv + 1);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to open configuration file");

    while ((c = getopt_long(argc, argv, "c:", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'c':
            /* already parsed */
            break;
        case '?':
        default:
            /* getopt_long() prints meaningful error messages itself */
            exit(EX_USAGE);
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1)
        error(EX_USAGE, 0, "not enough arguments");
    if (argc > 1)
        error(EX_USAGE, 0, "unexpected argument: %s", argv[1]);

    backend = rbh_backend_from_uri(argv[0], false);

    update_path();

    return EXIT_SUCCESS;
}
