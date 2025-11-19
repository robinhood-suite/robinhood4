/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include <robinhood.h>
#include <robinhood/open.h>

static struct rbh_backend *backend;
int mount_fd = -1;

static void __attribute__((destructor))
close_mountpoint(void)
{
    if (mount_fd != -1)
        close(mount_fd);
}

static void __attribute__((destructor))
destroy_backend(void)
{
    if (backend)
        rbh_backend_destroy(backend);
}

static void
usage(void)
{
    const char *message =
        "Usage: %s [-h] BACKEND\n"
        "\n"
        "Iterate on a robinhood BACKEND's entries ready for garbage collection.\n"
        "If these entries are absent from the filesystem, delete them from "
        "BACKEND for good.\n"
        "\n"
        "Positional arguments:\n"
        "    BACKEND  a URI describing a robinhood backend\n"
        "\n"
        "Optional arguments:\n"
        "    -d, --dry-run  displays the list of the absent entries\n"
        "    -h, --help     print this messsage and exit\n";

    printf(message, program_invocation_short_name);
}

static char *
get_mountpoint_from_source(struct rbh_backend *source)
{
    struct rbh_value_map *mountpoint_value_map;
    char *mountpoint;

    mountpoint_value_map = rbh_backend_get_info(source,
                                                RBH_INFO_MOUNTPOINT);
    if (mountpoint_value_map == NULL || mountpoint_value_map->count != 1) {
        fprintf(stderr, "Failed to get mountpoint from source URI\n");
        return NULL;
    }

    mountpoint = strdup(mountpoint_value_map->pairs[0].value->string);
    if (mountpoint == NULL)
        error(EXIT_FAILURE, errno, "Failed to allocate mountpoint");

    return mountpoint;
}

struct fsentry2delete_iterator {
    struct rbh_iterator iterator;

    struct rbh_iterator *fsentries;
    struct rbh_fsevent delete;
};

static const void *
fsentry2delete_iter_next(void *iterator)
{
    struct fsentry2delete_iterator *deletes = iterator;

    while (true) {
        const struct rbh_fsentry *fsentry;
        int fd;

        fsentry = rbh_iter_next(deletes->fsentries);
        if (fsentry == NULL)
            return NULL;

        assert((fsentry->mask & RBH_FP_ID) == RBH_FP_ID);

        errno = 0;

        fd = open_by_id_generic(mount_fd, &fsentry->id);
        if (fd < 0 && errno != ENOENT && errno != ESTALE)
            /* Something happened, something bad... */
            error(EXIT_FAILURE, errno, "open_by_id_generic");

        if (fd >= 0) {
            /* The entry still exists somewhere in the filesystem
             *
             * Let's not delete it yet.
             */
            if (close(fd))
                /* This should never happen */
                error(EXIT_FAILURE, errno, "unexpected error on close");
            continue;
        }

        deletes->delete.id = fsentry->id;
        return &deletes->delete;
    }
}

static void
fsentry2delete_iter_destroy(void *iterator)
{
    struct fsentry2delete_iterator *deletes = iterator;

    rbh_iter_destroy(deletes->fsentries);
    free(deletes);
}

static const struct rbh_iterator_operations FSENTRY2DELETE_ITER_OPS = {
    .next = fsentry2delete_iter_next,
    .destroy = fsentry2delete_iter_destroy,
};

static const struct rbh_iterator FSENTRY2DELETE_ITERATOR = {
    .ops = &FSENTRY2DELETE_ITER_OPS,
};

static struct rbh_iterator *
iter_fsentry2delete(struct rbh_iterator *fsentries)
{
    struct fsentry2delete_iterator *deletes;

    deletes = malloc(sizeof(*deletes));
    if (deletes == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    deletes->iterator = FSENTRY2DELETE_ITERATOR;
    deletes->fsentries = fsentries;
    deletes->delete.type = RBH_FET_DELETE;
    return &deletes->iterator;
}

static void
gc(bool dry_run_mode, bool verbose_mode)
{
    const struct rbh_filter_options OPTIONS = {
        .dry_run = dry_run_mode,
        .verbose = verbose_mode,
    };
    const struct rbh_filter_output OUTPUT = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = RBH_FP_ID,
        },
    };
    struct rbh_mut_iterator *fsentries;
    struct rbh_iterator *constify;
    struct rbh_iterator *deletes;

    fsentries = rbh_backend_filter(backend, NULL, &OPTIONS, &OUTPUT, NULL);
    if (fsentries == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_filter");

    constify = rbh_iter_constify(fsentries);
    deletes = iter_fsentry2delete(constify);

    if (rbh_backend_update(backend, deletes) == -1)
        error(EXIT_FAILURE, errno, "rbh_backend_update");

    rbh_iter_destroy(deletes);
}

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "dry-run",
            .val = 'd',
        },
        {
            .name = "help",
            .val = 'h',
        },
        {
            .name = "verbose",
            .val = 'v',
        },
        {}
    };
    bool dry_run_mode = false;
    bool verbose_mode = false;
    char *path;
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "dhv", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'd':
            dry_run_mode = true;
            break;
        case 'h':
            usage();
            return 0;
        case 'v':
            verbose_mode = true;
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

    /* Parse BACKEND */
    backend = rbh_backend_from_uri(argv[0], true);

    /* Retrieve mountpoint */
    path = get_mountpoint_from_source(backend);

    mount_fd = open(path, O_RDONLY | O_CLOEXEC);
    if (mount_fd < 0)
        error(EXIT_FAILURE, errno, "open: %s", argv[1]);

    gc(dry_run_mode, verbose_mode);
    return EXIT_SUCCESS;
}
