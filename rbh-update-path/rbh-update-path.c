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
                        RBH_FP_NAMESPACE_XATTRS,
        .statx_mask = 0,
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
    struct rbh_mut_iterator *fsentries;
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

    fsentries = get_entry_without_path();

    while (true) {
        struct rbh_fsentry *fsentry = rbh_mut_iter_next(fsentries);

        if (fsentry == NULL) {
            if (errno == ENODATA)
                break;

            if (errno == RBH_BACKEND_ERROR)
                error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
            else
                error(EXIT_FAILURE, errno, "failed to retrieve fsentry");
        }

        printf("%s\n", fsentry->name);

        free(fsentry);
    }

    rbh_mut_iter_destroy(fsentries);

    return EXIT_SUCCESS;
}
