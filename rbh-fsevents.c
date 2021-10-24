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

static void
usage(void)
{
    const char *message =
        "usage: %s [-h] [--raw] [--enrich MOUNTPOINT] SOURCE DESTINATION\n"
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
        "\n"
        "Note that uploading raw records to a RobinHood backend will fail, they have to\n"
        "be enriched first.\n";

    printf(message, program_invocation_short_name);
}

static struct source *
source_new(const char *arg)
{
    FILE *file;

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

    /* TODO: parse SOURCE as an MDT name (ie. <fsname>-MDT<index>) */
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

static int mount_fd = -1;

static void __attribute__((destructor))
mount_fd_exit(void)
{
    if (mount_fd != -1)
        /* Ignore errors on close */
        close(mount_fd);
}

static const size_t BATCH_SIZE = 1;

static void
feed(struct sink *sink, struct source *source, bool enrich, bool allow_partials)
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

        if (enrich)
            fsevents = iter_enrich(fsevents, mount_fd);
        else if (!allow_partials)
            fsevents = iter_no_partial(fsevents);
        if (fsevents == NULL)
            error(EXIT_FAILURE, errno, "iter_enrich");

        if (sink_process(sink, fsevents))
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
            .name = "raw",
            .val = 'r',
        },
        {}
    };
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "e:hr", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'e':
            mount_fd = open(optarg, O_RDONLY | O_CLOEXEC);
            if (mount_fd == -1)
                error(EXIT_FAILURE, errno, "open: %s", optarg);
            break;
        case 'h':
            usage();
            return 0;
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

    source = source_new(argv[optind++]);
    sink = sink_new(argv[optind++]);

    feed(sink, source, mount_fd != -1, strcmp(sink->name, "backend"));
    return error_message_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
