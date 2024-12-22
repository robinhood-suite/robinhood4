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
#include <robinhood/config.h>
#include <robinhood/alias.h>

#include "deduplicator.h"
#include "enricher.h"
#include "source.h"
#include "sink.h"

struct deduplicator_options {
    size_t batch_size;
};

static const size_t DEFAULT_BATCH_SIZE = 100;

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
        "                        '-' for stdin;\n"
        "                        a Source URI (eg. src:file:/path/to/test, \n"
        "                        src:lustre:lustre-MDT0000,\n"
        "                        src:hestia:/path/to/file).\n"
        "    DESTINATION     can be one of:\n"
        "                        '-' for stdout;\n"
        "                        a RobinHood URI (eg. rbh:mongo:test).\n"
        "\n"
        "Optional arguments:\n"
        "    --alias NAME    specify an alias for the operation.\n"
        "    -b, --batch-size NUMBER\n"
        "                    the number of fsevents to keep in memory for deduplication\n"
        "                    default: %lu\n"
        "                    the path to a configuration file\n"
        "    -c, --config PATH\n"
        "                    the configuration file to use.\n"
        "    --dry-run       displays the command after alias management\n"
        "    -d, --dump PATH\n"
        "                    the path to a file where the changelogs should be dumped,\n"
        "                    can only be used with a Lustre source. Use '-' for stdout.\n"
        "    -e, --enrich MOUNTPOINT\n"
        "                    enrich changelog records by querying MOUNTPOINT as needed\n"
        "                    MOUNTPOINT is a RobinHood URI (eg. rbh:lustre:/mnt/lustre)\n"
        "    -h, --help      print this message and exit\n"
        "    -l, --lustre    consider SOURCE is an MDT name\n"
        "    -r, --raw       do not enrich changelog records (default)\n"
        "\n"
        "Note that uploading raw records to a RobinHood backend will fail, they have to\n"
        "be enriched first.\n";

    printf(message, program_invocation_short_name, DEFAULT_BATCH_SIZE);
}

static char *
parse_query(const char *query)
{
    const char *key = query;
    char *value;
    char *equal;

    equal = strchr(query, '=');
    if (equal == NULL)
        error(EXIT_FAILURE, EINVAL, "URI's query should be of the form 'key=value', got '%s'",
              query);

    value = equal + 1;
    if (strncmp(key, "ack-user", strlen("ack-user")))
        error(EXIT_FAILURE, EINVAL, "URI's query should only contain the option 'ack-user=<user>', option '%s' unknown",
              query);

    return value;
}

static struct source *
source_from_file_uri(const char *file_path,
                     struct source *(*source_from)(FILE *))
{
    FILE *file;

    file = fopen(file_path, "r");
    if (file != NULL)
        /* SOURCE is a path to a file */
        return source_from(file);

    if (file == NULL && errno != ENOENT)
        /* SOURCE is a path to a file, but there was some sort of error trying
         * to open it.
         */
        error(EXIT_FAILURE, errno, "%s", file_path);

    error(EX_USAGE, EINVAL, "%s", file_path);
    __builtin_unreachable();
}

static struct source *
source_from_uri(const char *uri, const char *dump_file)
{
    struct source *source = NULL;
    struct rbh_raw_uri *raw_uri;
    char *name = NULL;
    char *username;
    char *colon;

    (void) dump_file;

    raw_uri = rbh_raw_uri_from_string(uri);
    if (raw_uri == NULL)
        error(EXIT_FAILURE, errno, "cannot parse URI '%s'", uri);

    if (strcmp(raw_uri->scheme, RBH_SOURCE) != 0) {
        free(raw_uri);
        error(EX_USAGE, 0, "%s: uri scheme not supported", raw_uri->scheme);
        __builtin_unreachable();
    }

    colon = strrchr(raw_uri->path, ':');
    if (colon) {
        /* colon = backend_type:fsname */
        *colon = '\0';
        name = colon + 1;
    }

    if (raw_uri->query)
        username = parse_query(raw_uri->query);
    else
        username = NULL;

    if (strcmp(raw_uri->path, "file") == 0) {
        source = source_from_file_uri(name, source_from_file);
    } else if (strcmp(raw_uri->path, "lustre") == 0) {
#ifdef HAVE_LUSTRE
        source = source_from_lustre_changelog(name, username, dump_file);
#else
        free(raw_uri);
        error(EX_USAGE, EINVAL, "MDT source is not available");
        __builtin_unreachable();
#endif
    } else if (strcmp(raw_uri->path, "hestia") == 0) {
        source = source_from_file_uri(name, source_from_hestia_file);
    }

    free(raw_uri);

    if (source)
        return source;

    error(EX_USAGE, 0, "%s: uri path not supported", raw_uri->path);
    __builtin_unreachable();
}

static struct source *
source_new(const char *arg, const char *dump_file)
{
    if (strcmp(arg, "-") == 0)
        /* SOURCE is '-' (stdin) */
        return source_from_file(stdin);

    if (rbh_is_uri(arg))
        return source_from_uri(arg, dump_file);

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
        error(EXIT_FAILURE, errno, "cannot parse URI '%s'", uri);

    if (strcmp(raw_uri->scheme, "rbh") == 0) {
        free(raw_uri);
        return (void *) sink_from_backend(rbh_backend_from_uri(uri));
    }

    free(raw_uri);
    error(EX_USAGE, 0, "%s: uri scheme not supported", uri);
    __builtin_unreachable();
}

static struct sink *
sink_new(const char *arg)
{
    if (strcmp(arg, "-") == 0)
        /* DESTINATION is '-' (stdout) */
        return sink_from_file(stdout);

    if (rbh_is_uri(arg))
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
    struct rbh_backend *uri_backend;
    struct rbh_raw_uri *raw_uri;
    struct rbh_uri *rbh_uri;
    int save_errno;

    raw_uri = rbh_raw_uri_from_string(uri);
    if (raw_uri == NULL)
        error(EXIT_FAILURE, errno, "cannot parse URI '%s'", uri);

    rbh_uri = rbh_uri_from_raw_uri(raw_uri);
    save_errno = errno;
    free(raw_uri);
    errno = save_errno;
    if (rbh_uri == NULL)
        error(EXIT_FAILURE, errno, "cannot parse URI '%s'", uri);

    /* XXX: this a temporary hack because the Hestia backend cannot properly
     * build at the moment.
     */
    if (strlen(rbh_uri->fsname) == 0) {
        uri_backend = malloc(sizeof(*uri_backend));
        if (uri_backend == NULL)
            error(EXIT_FAILURE, errno, "malloc");

        uri_backend->id = RBH_BI_HESTIA;
    } else {
        uri_backend = rbh_backend_from_uri(uri);
    }

    builder = enrich_iter_builder_from_backend(rbh_uri->backend, uri_backend,
                                               rbh_uri->fsname);
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

static struct rbh_backend *enrich_point;

static void __attribute__((destructor))
destroy_enrich_point(void)
{
    if (enrich_point)
        rbh_backend_destroy(enrich_point);
}

static bool skip_error = true;

static void
feed(struct sink *sink, struct source *source,
     struct enrich_iter_builder *builder, bool allow_partials,
     struct deduplicator_options *dedup_opts)
{
    struct rbh_mut_iterator *deduplicator;

    deduplicator = deduplicator_new(dedup_opts->batch_size, source);
    if (deduplicator == NULL)
        error(EXIT_FAILURE, errno, "deduplicator_new");

    while (true) {
        struct rbh_iterator *fsevents;

        errno = 0;
        fsevents = rbh_mut_iter_next(deduplicator);
        if (fsevents == NULL)
            break;

        if (builder != NULL)
            fsevents = build_enrich_iter(builder, fsevents, skip_error);
        else if (!allow_partials)
            fsevents = iter_no_partial(fsevents);

        if (fsevents == NULL)
            error(EXIT_FAILURE, errno, "iter_enrich");

        if (sink_process(sink, fsevents))
            break;

        rbh_iter_destroy(fsevents);
    }

    switch (errno) {
    case 0:
        error(EXIT_FAILURE, EINVAL, "unexpected exit status 0");
    case ENODATA:
        break;
    case RBH_BACKEND_ERROR:
        error(EXIT_FAILURE, 0, "%s\n", rbh_backend_error);
        __builtin_unreachable();
    default:
        error(EXIT_FAILURE, errno, "could not get the next batch of fsevents");
    }

    rbh_mut_iter_destroy(deduplicator);
}

static bool
str2size_t(const char *input, size_t *result)
{
    char *end;

    errno = 0;
    *result = strtoull(input, &end, 0);
    if (errno) {
        return -1;
    } else if ((!*result && input == end) || *end != '\0') {
        errno = EINVAL;
        return false;
    }

    return true;
}

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "batch-size",
            .has_arg = required_argument,
            .val = 'b',
        },
        {
            .name = "config",
            .has_arg = required_argument,
            .val = 'c',
        },
        {
            .name = "dump",
            .has_arg = required_argument,
            .val = 'd',
        },
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
            .name = "no-skip",
            .val = 'n',
        },
        {
            .name = "raw",
            .val = 'r',
        },
        {
            .name = "dry-run",
            .has_arg = no_argument,
            .val = 'x',
        },
        {}
    };
    struct deduplicator_options dedup_opts = {
        .batch_size = DEFAULT_BATCH_SIZE,
    };
    char *dump_file = NULL;
    int rc;
    char c;

    rc = rbh_config_from_args(argc - 1, argv + 1);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to load configuration file");

    rbh_apply_aliases(&argc, &argv);

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "b:c:d:e:hnr", LONG_OPTIONS,
                            NULL)) != -1) {
        switch (c) {
        case 'b':
            if (!str2size_t(optarg, &dedup_opts.batch_size))
                error(EXIT_FAILURE, 0, "'%s' is not an integer", optarg);

            break;
        case 'c':
            /* already parsed */
            break;
        case 'd':
            dump_file = strdup(optarg);
            if (dump_file == NULL)
                error(EXIT_FAILURE, ENOMEM, "strdup");
            break;
        case 'e':
            enrich_builder = enrich_iter_builder_from_uri(optarg);
            if (enrich_builder == NULL)
                error(EXIT_FAILURE, errno, "invalid enrich URI '%s'", optarg);
            break;
        case 'h':
            usage();
            return 0;
        case 'n':
            skip_error = false;
            break;
        case 'r':
            /* Ignore errors on close */
            mount_fd_exit();
            mount_fd = -1;
            break;
        case 'x':
            rbh_display_resolved_argv(NULL, &argc, &argv);
            return EXIT_SUCCESS;
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

    if (dump_file && strcmp(argv[optind + 1], dump_file) == 0)
        error(EX_USAGE, EINVAL,
              "Cannot output changelogs and fsevents both to stdout");

    source = source_new(argv[optind++], dump_file);
    sink = sink_new(argv[optind++]);

    feed(sink, source, enrich_builder, strcmp(sink->name, "backend"),
         &dedup_opts);

    rbh_config_free();

    return error_message_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
