/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <error.h>
#include <getopt.h>
#include <sysexits.h>

#include "rbh_update_path.h"
#include "utils.h"

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
get_entry_without_path()
{
    const struct rbh_filter_field *field = str2filter_field("ns-xattrs.path");
    struct rbh_filter *filter_path;
    struct rbh_filter *filter;

    filter_path = rbh_filter_exists_new(field);
    if (filter_path == NULL)
        error(EXIT_FAILURE, errno, "failed to create path filter");

    filter = rbh_filter_not(filter_path);

    return get_entries(backend, filter);
}

static bool
update_path()
{
    struct rbh_mut_iterator *fsentries;
    struct rbh_mut_iterator *batch;
    struct rbh_list_node *batches;
    struct rbh_fsentry *entry;
    bool from_mirror = true;
    bool empty = true;
    int rc;

    batches = xmalloc(sizeof(*batches));
    rbh_list_init(batches);

    /* Retrieve initial batch: all entries without a path from mirror backend */
    fsentries = get_entry_without_path();
    add_iterator(batches, fsentries);

    /* The first batch is from the mirror backend, subsequent batches are
     * children discovered during processing.
     */
    for (batch = get_iterator(batches); batch != NULL;
         batch = get_iterator(batches)) {
        struct rbh_list_node *fsevents = xmalloc(sizeof(*fsevents));
        rbh_list_init(fsevents);

        for (entry = rbh_mut_iter_next(batch); entry != NULL;
             entry = rbh_mut_iter_next(batch)) {
            struct rbh_fsevent *fsevent;

            /* Mark that we've processed at least one entry. This indicates
             * update_path() should be called again until there is no more
             * entry without a path.
             */
            if (empty)
                empty = false;

            /* If it's not a directory, no need to update it's children */
            if (S_ISDIR(entry->statx->stx_mode)) {
                if(remove_children_path(backend, entry, batches))
                    error(EXIT_FAILURE, errno, "failed to remove children path");
            }

            fsevent = get_entry_path(backend, entry);
            /* Means that entry doesn't have a parent yet or its parent doesn't
             * have yet a path
             */
            if (fsevent == NULL && errno == ENODATA)
                continue;

            add_data_list(fsevents, fsevent);

            /* Entries in child batches are freed when their iterator is
             * destroyed, so we only free the initial mirror entries here.
             */
            if (from_mirror)
                free(entry);
        }

        switch (errno) {
        case ENODATA:
            break;
        case RBH_BACKEND_ERROR:
            error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
        default:
            error(EXIT_FAILURE, errno, "failed to retrieve entry");
        }

        if (!rbh_list_empty(fsevents)) {
            struct rbh_iterator *iter = _rbh_iter_list(fsevents);
            rc = chunkify_update(iter, backend);
            if (rc)
                error(EXIT_FAILURE, errno, "failed to update path");
        } else {
            free(fsevents);
        }

        rbh_mut_iter_destroy(batch);
        /* After processing the first batch, all subsequent batches are
         * children, not from the mirror backend.
         */
        from_mirror = false;
    }

    free(batches);

    return empty;
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

    while (!update_path());

    return EXIT_SUCCESS;
}
