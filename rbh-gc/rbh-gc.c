/* This file is part of Robinhood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
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

#ifndef RBH_ITER_CHUNK_SIZE
# define RBH_ITER_CHUNK_SIZE (1 << 12)
#endif

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
        "    -d, --dry-run              displays the list of the absent entries\n"
        "    -h, --help                 print this messsage and exit\n"
        "    -s, --sync-time SYNC_TIME  instead of checking every entry of the BACKEND,\n"
        "                               only consider entries with a sync_time lesser\n"
        "                               than SYNC_TIME\n"
        "    -v, --verbose              verbose mode\n";

    printf(message, program_invocation_short_name);
}

static char *
get_mountpoint_from_source(struct rbh_backend *source)
{
    struct rbh_value_map *mountpoint_value_map;

    mountpoint_value_map = rbh_backend_get_info(source,
                                                RBH_INFO_MOUNTPOINT);
    if (mountpoint_value_map == NULL || mountpoint_value_map->count != 1) {
        fprintf(stderr, "Failed to get mountpoint from source URI\n");
        return NULL;
    }

    return xstrdup(mountpoint_value_map->pairs[0].value->string);
}

struct fsentry2gc_iterator {
    struct rbh_iterator iterator;

    struct rbh_iterator *fsentries;
    struct rbh_fsevent delete;
};

static bool
still_alive(const struct rbh_id *id)
{
    int fd;

    errno = 0;

    fd = open_by_id_opath(mount_fd, id);
    if (fd < 0 && errno != ENOENT && errno != ESTALE)
        /* Something happened, something bad... */
        error(EXIT_FAILURE, errno, "open_by_id_opath");

    if (fd >= 0) {
        /* The entry still exists somewhere in the filesystem
         *
         * Let's not delete it yet.
         */
        if (close(fd))
            /* This should never happen */
            error(EXIT_FAILURE, errno, "unexpected error on close");
        return true;
    }

    return false;
}

static const void *
fsentry2delete_iter_next(void *iterator)
{
    struct fsentry2gc_iterator *deletes = iterator;

    while (true) {
        const struct rbh_fsentry *fsentry;

        fsentry = rbh_iter_next(deletes->fsentries);
        if (fsentry == NULL)
            return NULL;

        assert((fsentry->mask & RBH_FP_ID) == RBH_FP_ID);

        if (still_alive(&fsentry->id))
            continue;

        deletes->delete.id = fsentry->id;
        return &deletes->delete;
    }

    return NULL;
}

static const void *
fsentry2print_iter_next(void *iterator)
{
    struct fsentry2gc_iterator *prints = iterator;

    while (true) {
        const struct rbh_fsentry *fsentry;

        fsentry = rbh_iter_next(prints->fsentries);
        if (fsentry == NULL)
            return NULL;

        assert((fsentry->mask & RBH_FP_ID) == RBH_FP_ID);

        if (still_alive(&fsentry->id))
            continue;

        return fsentry;
    }

    return NULL;
}

static void
fsentry2gc_iter_destroy(void *iterator)
{
    struct fsentry2gc_iterator *iter = iterator;

    rbh_iter_destroy(iter->fsentries);
    free(iter);
}

static const struct rbh_iterator_operations FSENTRY2DELETE_ITER_OPS = {
    .next = fsentry2delete_iter_next,
    .destroy = fsentry2gc_iter_destroy,
};

static const struct rbh_iterator_operations FSENTRY2PRINT_ITER_OPS = {
    .next = fsentry2print_iter_next,
    .destroy = fsentry2gc_iter_destroy,
};

static const struct rbh_iterator FSENTRY2DELETE_ITERATOR = {
    .ops = &FSENTRY2DELETE_ITER_OPS,
};

static const struct rbh_iterator FSENTRY2PRINT_ITERATOR = {
    .ops = &FSENTRY2PRINT_ITER_OPS,
};

static struct rbh_iterator *
iter_fsentry2delete(struct rbh_iterator *fsentries)
{
    struct fsentry2gc_iterator *deletes;

    deletes = xmalloc(sizeof(*deletes));

    deletes->iterator = FSENTRY2DELETE_ITERATOR;
    deletes->fsentries = fsentries;
    deletes->delete.type = RBH_FET_DELETE;
    return &deletes->iterator;
}

static struct rbh_iterator *
iter_fsentry2print(struct rbh_iterator *fsentries)
{
    struct fsentry2gc_iterator *prints;

    prints = xmalloc(sizeof(*prints));

    prints->iterator = FSENTRY2PRINT_ITERATOR;
    prints->fsentries = fsentries;
    return &prints->iterator;
}

static int
print_entries(struct rbh_iterator *iterator)
{
    int save_errno = errno;
    size_t count = 0;

    if (iterator == NULL)
        return 0;

    do {
        const struct rbh_fsentry *entry;

        entry = rbh_iter_next(iterator);
        if (entry == NULL) {
            if (errno == ENODATA)
                break;

            return -1;
        }

        printf("'%s' needs to be deleted\n",
               rbh_fsentry_find_ns_xattr(entry, "path")->string);
        count++;
    } while (true);

    printf("%lu element%s total to delete\n", count, count > 1 ? "s" : "");

    errno = save_errno;
    return count;
}

static void
gc(bool dry_run_mode, bool verbose_mode, int64_t sync_time)
{
    const struct rbh_filter_options OPTIONS = {
        .verbose = verbose_mode,
    };
    const struct rbh_filter_output OUTPUT = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = dry_run_mode ?
                RBH_FP_ID | RBH_FP_NAMESPACE_XATTRS :
                RBH_FP_ID,
        },
    };
    const struct rbh_filter_field *field;
    struct rbh_mut_iterator *fsentries;
    struct rbh_filter *filter = NULL;
    struct rbh_iterator *constify;

    field = str2filter_field("ns-xattrs.sync_time");

    if (sync_time >= 0) {
        filter = rbh_filter_compare_int64_new(RBH_FOP_STRICTLY_LOWER, field,
                                              sync_time);
        if (filter == NULL)
            error(EXIT_FAILURE, errno, "sync_time2filter");
    }

    fsentries = rbh_backend_filter(backend, filter, &OPTIONS, &OUTPUT, NULL);
    if (fsentries == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_filter");

    free(filter);

    constify = rbh_iter_constify(fsentries);

    if (!dry_run_mode) {
        struct rbh_mut_iterator *chunks;
        struct rbh_iterator *deletes;

        deletes = iter_fsentry2delete(constify);

        chunks = rbh_iter_chunkify(deletes, RBH_ITER_CHUNK_SIZE);
        if (chunks == NULL)
            error(EXIT_FAILURE, errno, "rbh_iter_chunkify");

        do {
            struct rbh_iterator *chunk = rbh_mut_iter_next(chunks);
            int save_errno;
            ssize_t count;

            if (chunk == NULL) {
                if (errno == ENODATA || errno == RBH_BACKEND_ERROR)
                    break;
                error(EXIT_FAILURE, errno, "while chunkifying entries");
            }

            count = rbh_backend_update(backend, chunk);
            save_errno = errno;
            rbh_iter_destroy(chunk);
            if (count < 0) {
                errno = save_errno;
                assert(errno != ENODATA);
                break;
            }
        } while (true);

        switch (errno) {
        case ENODATA:
            rbh_backend_update(backend, NULL);
            break;
        case RBH_BACKEND_ERROR:
            error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
            __builtin_unreachable();
        default:
            error(EXIT_FAILURE, errno, "while iterating over entries");
        }
    } else {
        struct rbh_iterator *prints;

        prints = iter_fsentry2print(constify);
        if (print_entries(prints) == -1)
            error(EXIT_FAILURE, errno, "print_entries");

        rbh_iter_destroy(prints);
    }
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
            .name = "sync-time",
            .val = 's',
            .has_arg = required_argument,
        },
        {
            .name = "verbose",
            .val = 'v',
        },
        {}
    };
    bool dry_run_mode = false;
    bool verbose_mode = false;
    int64_t sync_time = -1;
    char *path;
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "dhs:v", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'd':
            dry_run_mode = true;
            break;
        case 'h':
            usage();
            return 0;
        case 's':
            if (str2int64_t(optarg, &sync_time))
                error(EXIT_FAILURE, errno, "str2int64_t");
            break;
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
    free(path);
    if (mount_fd < 0)
        error(EXIT_FAILURE, errno, "open: %s", argv[1]);

    gc(dry_run_mode, verbose_mode, sync_time);
    return EXIT_SUCCESS;
}
