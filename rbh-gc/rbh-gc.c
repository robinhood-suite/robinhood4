/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include <robinhood.h>
#include <robinhood/config.h>
#include <robinhood/filters/parser.h>
#include <robinhood/open.h>
#include <robinhood/utils.h>

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
        "    -c, --config PATH          the path to a configuration file\n"
        "    -d, --dry-run              displays the list of the absent entries\n"
        "    -h, --help                 print this messsage and exit\n"
        "    -s, --sync-time SYNC_TIME  instead of checking every entry of the BACKEND,\n"
        "                               only consider entries with a sync_time lesser\n"
        "                               than SYNC_TIME\n"
        "    -v, --verbose              verbose mode\n"
        "    --version                  print RobinHood 4's version\n";

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
    const char *mnt_path;
};

static bool
still_alive(const struct rbh_id *id, const char *path, const char *mnt_path,
            uint32_t nlink)
{
    int fd;
    int rc;

    errno = 0;

    fd = open_by_id_opath(mount_fd, id);
    if (fd < 0 && errno != ENOENT && errno != ESTALE)
        /* Something happened, something bad... */
        error(EXIT_FAILURE, errno, "open_by_id_opath");

    if (fd < 0)
        /* The entry does not exist, delete it */
        return false;

    if (close(fd))
        /* This should never happen */
        error(EXIT_FAILURE, errno, "unexpected error on close");

    if (nlink > 1) {
        char buffer[PATH_MAX];
        /* Verifying the path still exists in case the handle
         * had multiple hardlinks
         *
         * We cannot use `open_by_id()` for this one, as we need
         * to directly target the path, so calling `access()` is enough
         */
        rc = snprintf(buffer, PATH_MAX, "%s%s", mnt_path, path);
        if (rc >= PATH_MAX)
            error(EXIT_FAILURE, errno, "path build failed");
        if (access(buffer, F_OK))
            if (errno == ENOENT)
                return false;
    }

    return true;
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

        if (still_alive(
            &fsentry->id,
            rbh_fsentry_find_ns_xattr(fsentry, "path")->string,
            deletes->mnt_path,
            fsentry->statx->stx_nlink))
            continue;

        deletes->delete.id = fsentry->id;
        deletes->delete.link.parent_id = &fsentry->parent_id;
        deletes->delete.link.name = fsentry->name;
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

        if (still_alive(
            &fsentry->id,
            rbh_fsentry_find_ns_xattr(fsentry, "path")->string,
            prints->mnt_path,
            fsentry->statx->stx_nlink))
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
iter_fsentry2delete(struct rbh_iterator *fsentries, const char *mnt_path)
{
    struct fsentry2gc_iterator *deletes;

    deletes = xmalloc(sizeof(*deletes));

    deletes->iterator = FSENTRY2DELETE_ITERATOR;
    deletes->fsentries = fsentries;
    deletes->delete.type = RBH_FET_UNLINK;
    deletes->mnt_path = mnt_path;

    return &deletes->iterator;
}

static struct rbh_iterator *
iter_fsentry2print(struct rbh_iterator *fsentries, const char *mnt_path)
{
    struct fsentry2gc_iterator *prints;

    prints = xmalloc(sizeof(*prints));

    prints->iterator = FSENTRY2PRINT_ITERATOR;
    prints->fsentries = fsentries;
    prints->mnt_path = mnt_path;

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
gc(char *mnt_path, bool dry_run_mode, bool verbose_mode, int64_t sync_time,
   struct rbh_filter *filter)
{
    const struct rbh_filter_options OPTIONS = {
        .verbose = verbose_mode,
    };
    const struct rbh_filter_output OUTPUT = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = dry_run_mode ?
                RBH_FP_ID | RBH_FP_NAMESPACE_XATTRS | RBH_FP_STATX:
                RBH_FP_ID | RBH_FP_NAMESPACE_XATTRS | RBH_FP_STATX |
                    RBH_FP_PARENT_ID | RBH_FP_NAME,
            .statx_mask = RBH_STATX_NLINK,
        },
    };
    struct rbh_mut_iterator *fsentries;
    const struct rbh_filter *AND_FILTERS[2];
    struct rbh_filter and_filter = {
        .op = RBH_FOP_AND,
        .logical = {
            .filters = AND_FILTERS,
            .count = sizeof(AND_FILTERS) / sizeof(*AND_FILTERS)
        }
    };
    struct rbh_filter *_filter = NULL;
    struct rbh_iterator *constify;

    if (sync_time >= 0) {
        const struct rbh_filter_field *field;

        field = str2filter_field("ns-xattrs.sync_time");
        _filter = rbh_filter_compare_int64_new(RBH_FOP_STRICTLY_LOWER, field,
                                               sync_time);
        if (_filter == NULL)
            error(EXIT_FAILURE, errno, "sync_time2filter");

        if (filter == NULL) {
            filter = _filter;
        } else {
            AND_FILTERS[0] = filter;
            AND_FILTERS[1] = _filter;
            filter = &and_filter;
        }
    }

    fsentries = rbh_backend_filter(backend, filter, &OPTIONS, &OUTPUT, NULL);
    if (fsentries == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_filter");

    free(_filter);

    constify = rbh_iter_constify(fsentries);

    if (!dry_run_mode) {
        struct rbh_mut_iterator *chunks;
        struct rbh_iterator *deletes;

        deletes = iter_fsentry2delete(constify, mnt_path);

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

        prints = iter_fsentry2print(constify, mnt_path);
        if (print_entries(prints) == -1)
            error(EXIT_FAILURE, errno, "print_entries");

        rbh_iter_destroy(prints);
    }
}

int
main(int _argc, char *_argv[])
{
    struct rbh_filter_options options = {0};
    struct filters_context f_ctx = {0};
    struct rbh_value_map *info_map;
    bool dry_run_mode = false;
    bool verbose_mode = false;
    struct rbh_filter *filter;
    int64_t sync_time = -1;
    int others_count = 0;
    char **others = NULL;
    int index = 1;
    char **argv;
    char *path;
    int argc;
    int rc;

    argc = _argc - 1;
    argv = &_argv[1];

    rc = rbh_config_from_args(argc, argv);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to load configuration file");

    others = xmalloc(sizeof(char*) * argc);

    /* Parse the command line */
    for (int i = 0; i < argc; ++i) {
        char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            usage();
            return EXIT_SUCCESS;

        } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-z") == 0) {
            rbh_print_version();
            return EXIT_SUCCESS;

        } else if (strcmp(arg, "--config") == 0 || strcmp(arg, "-c") == 0) {
            /* already parsed */
            ++i;

        } else if (strcmp(arg, "--dry-run") == 0 || strcmp(arg, "-d") == 0) {
            dry_run_mode = true;

        } else if (strcmp(arg, "--sync-time") == 0 || strcmp(arg, "-s") == 0) {
            if (i + 1 >= argc)
                error(EXIT_FAILURE, EINVAL, "Missing argument for %s", arg);

            if (str2int64_t(argv[++i], &sync_time))
                error(EXIT_FAILURE, errno, "str2int64_t");

        } else if (strcmp(arg, "--verbose") == 0 || strcmp(arg, "-v") == 0) {
            verbose_mode = true;

        } else {
            others[others_count++] = arg;
        }
    }

    argc = others_count;
    argv = others;

    if (argc < 1)
        error(EX_USAGE, 0, "not enough arguments");
    if (!rbh_is_uri(argv[0]))
        error(EX_USAGE, 0, "there is a filter before the URI: (%s)", argv[0]);

    /* Parse BACKEND */
    backend = rbh_backend_from_uri(argv[0], false);

    /* Retrieve source BACKEND */
    info_map = rbh_backend_get_info(backend, RBH_INFO_BACKEND_SOURCE);
    if (info_map == NULL)
        error(EXIT_FAILURE, errno,
              "Failed to retrieve the source backends from URI '%s', aborting",
              argv[0]);

    import_plugins(&f_ctx, &info_map, 1);
    f_ctx.need_prefetch = false;
    f_ctx.argc = argc;
    f_ctx.argv = argv;

    filter = parse_expression(&f_ctx, &index, NULL, NULL, NULL, NULL);
    if (index != f_ctx.argc)
        error(EX_USAGE, 0, "you have too many ')'");

    if (f_ctx.need_prefetch && complete_rbh_filter(filter, backend, &options))
        error(EXIT_FAILURE, errno, "Failed to complete filters");

    /* Retrieve mountpoint */
    path = get_mountpoint_from_source(backend);

    mount_fd = open(path, O_RDONLY | O_CLOEXEC);
    if (mount_fd < 0)
        error(EXIT_FAILURE, errno, "Failed to open mountpoint '%s'", path);

    gc(path, dry_run_mode, verbose_mode, sync_time, filter);

    free(path);
    rbh_config_free();

    return EXIT_SUCCESS;
}
