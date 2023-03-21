/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <robinhood/uri.h>
#include <robinhood/utils.h>

#include "deduplicator.h"
#include "enricher.h"
#include "source.h"
#include "sink.h"

enum rbh_source_t {
    SRC_DEFAULT = 0,
    SRC_FILE = SRC_DEFAULT,
    SRC_LUSTRE,
};

static void
usage(void)
{
    /* TODO: accept source as a URI or a '-', like src:lustre:lustre-MDT0000. */
    const char *message =
        "usage: %s [-h] [--raw] [--enrich MOUNTPOINT] [--lustre] SOURCE DESTINATION\n"
        "\n"
        "Collect changelog records from SOURCE, optionally enrich them with data\n"
        "collected from MOUNTPOINT and send them to DESTINATION.\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE          can be one of:\n"
        "                        a path to a yaml file, or '-' for stdin;\n"
        "                        an MDT name (eg. lustre-MDT0000).\n"
        "    DESTINATION     can be one of:\n"
        "                        '-' for stdout;\n"
        "                        a RobinHood URI (eg. rbh:mongo:test).\n"
        "\n"
        "Optional arguments:\n"
        "    -h, --help      print this message and exit\n"
        "    -r, --raw       do not enrich changelog records (default)\n"
        "    -e, --enrich MOUNTPOINT\n"
        "                    enrich changelog records by querying MOUNTPOINT as needed\n"
        "                    MOUNTPOINT is a RobinHood URI (eg. rbh:lustre:/mnt/lustre)\n"
        "    -l, --lustre    consider SOURCE is an MDT name\n"
        "\n"
        "Note that uploading raw records to a RobinHood backend will fail, they have to\n"
        "be enriched first.\n";

    printf(message, program_invocation_short_name);
}

static struct source *
source_new(const char *arg, enum  rbh_source_t source_type)
{
    FILE *file;

    switch(source_type) {
    case SRC_LUSTRE:
#ifdef HAVE_LUSTRE
        return source_from_lustre_changelog(arg);
#else
        error(EX_USAGE, EINVAL, "MDT source is not available");
#endif
    case SRC_FILE:
        break;
    default:
        __builtin_unreachable();
    }

    if (strcmp(arg, "-") == 0)
        /* SOURCE is '-' (stdin) */
        return source_from_file(stdin);

    file = fopen(arg, "r");
    if (file != NULL)
        /* SOURCE is a path to a file */
        return source_from_file(file);
    if (file == NULL && errno != ENOENT)
        /* SOURCE is a path to a file, but there was some sort of error trying
         * to open it.
         */
        error(EXIT_FAILURE, errno, "%s", arg);

    error(EX_USAGE, EINVAL, "%s", arg);
    __builtin_unreachable();
}

static struct source *source;

static void __attribute__((destructor))
source_exit(void)
{
    if (source)
        rbh_iter_destroy(&source->fsevents);
}

static struct sink *
sink_from_uri(const char *uri)
{
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(uri);
    if (raw_uri == NULL)
        error(EXIT_FAILURE, errno, "rbh_raw_uri_from_string");

    if (strcmp(raw_uri->scheme, "rbh") == 0) {
        free(raw_uri);
        return sink_from_backend(rbh_backend_from_uri(uri));
    }

    free(raw_uri);
    error(EX_USAGE, 0, "%s: uri scheme not supported", uri);
    __builtin_unreachable();
}

static bool
is_uri(const char *string)
{
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(string);
    if (raw_uri == NULL) {
        if (errno == EINVAL)
            return false;
        error(EXIT_FAILURE, errno, "rbh_raw_uri_from_string");
    }

    free(raw_uri);
    return true;
}

static struct sink *
sink_new(const char *arg)
{
    if (strcmp(arg, "-") == 0)
        /* DESTINATION is '-' (stdout) */
        return sink_from_file(stdout);

    if (is_uri(arg))
        return sink_from_uri(arg);

    error(EX_USAGE, EINVAL, "%s", arg);
    __builtin_unreachable();
}

static struct sink *sink;

static void __attribute__((destructor))
sink_exit(void)
{
    if (sink)
        sink_destroy(sink);
}

static struct enrich_iter_builder *
enrich_iter_builder_from_uri(const char *uri)
{
    struct enrich_iter_builder *builder;
    struct rbh_raw_uri *raw_uri;
    struct rbh_uri *rbh_uri;
    int save_errno;

    raw_uri = rbh_raw_uri_from_string(uri);
    if (raw_uri == NULL)
        error(EXIT_FAILURE, errno, "rbh_raw_uri_from_raw_uri: %s", uri);

    rbh_uri = rbh_uri_from_raw_uri(raw_uri);
    save_errno = errno;
    free(raw_uri);
    errno = save_errno;
    if (rbh_uri == NULL)
        error(EXIT_FAILURE, errno, "rbh_uri_from_raw_uri: %s", uri);

    builder = enrich_iter_builder_from_backend(
        rbh_backend_from_uri(uri), rbh_uri->fsname);
    save_errno = errno;
    free(rbh_uri);
    errno = save_errno;

    return builder;
}

static struct enrich_iter_builder *enrich_builder;

static void __attribute__((destructor))
enrich_iter_builder_exit(void)
{
    if (enrich_builder)
        enrich_iter_builder_destroy(enrich_builder);
}

static int mount_fd = -1;

static void __attribute__((destructor))
mount_fd_exit(void)
{
    if (mount_fd != -1)
        /* Ignore errors on close */
        close(mount_fd);
}

static const size_t BATCH_SIZE = 1;

static struct rbh_backend *enrich_point;

static void __attribute__((destructor))
destroy_enrich_point(void)
{
    if (enrich_point)
        rbh_backend_destroy(enrich_point);
}

static void
feed(struct sink *sink, struct source *source,
     struct enrich_iter_builder *builder, bool allow_partials)
{
    struct rbh_mut_iterator *deduplicator;

    deduplicator = deduplicator_new(BATCH_SIZE, source);
    if (deduplicator == NULL)
        error(EXIT_FAILURE, errno, "deduplicator_new");

    while (true) {
        struct rbh_iterator *fsevents;

        errno = 0;
        fsevents = rbh_mut_iter_next(deduplicator);
        if (fsevents == NULL)
            break;

        if (builder != NULL)
            fsevents = enrich_iter_builder_build_iter(builder, fsevents);
        else if (!allow_partials)
            fsevents = iter_no_partial(fsevents);

        if (fsevents == NULL)
            error(EXIT_FAILURE, errno, "iter_enrich");

        /* If we couldn't open the file because it is already deleted,
         * just ignore the error and manage the next record instead of quitting
         */
        if (sink_process(sink, fsevents) && errno != ESTALE)
            error(EXIT_FAILURE, errno, "sink_process");

        rbh_iter_destroy(fsevents);
    }

    if (errno != ENODATA)
        error(EXIT_FAILURE, errno, "getting the next batch of fsevents");

    rbh_mut_iter_destroy(deduplicator);
}

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "enrich",
            .has_arg = required_argument,
            .val = 'e',
        },
        {
            .name = "help",
            .val = 'h',
        },
        {
            .name = "lustre",
            .val = 'l',
        },
        {
            .name = "raw",
            .val = 'r',
        },
        {}
    };
    enum rbh_source_t source_type = SRC_DEFAULT;
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "e:hlr", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'e':
            enrich_builder = enrich_iter_builder_from_uri(optarg);
            if (enrich_builder == NULL)
                error(EXIT_FAILURE, errno, "enrich_new");
            break;
        case 'h':
            usage();
            return 0;
        case 'l':
            if (source_type != SRC_DEFAULT)
                error(EX_USAGE, EINVAL, "source type already specified");
            source_type = SRC_LUSTRE;
            break;
        case 'r':
            /* Ignore errors on close */
            mount_fd_exit();
            mount_fd = -1;
            break;
        case '?':
        default:
            /* getopt_long() prints meaningful error messages itself */
            exit(EX_USAGE);
        }
    }

    if (argc - optind < 2)
        error(EX_USAGE, 0, "not enough arguments");
    if (argc - optind > 2)
        error(EX_USAGE, 0, "too many arguments");

    source = source_new(argv[optind++], source_type);
    sink = sink_new(argv[optind++]);

    feed(sink, source, enrich_builder, strcmp(sink->name, "backend"));
    return error_message_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
