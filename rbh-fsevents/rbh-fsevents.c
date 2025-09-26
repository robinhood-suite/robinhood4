/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

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
#include <pthread.h>

#include <robinhood/uri.h>
#include <robinhood/utils.h>
#include <robinhood/config.h>
#include <robinhood/alias.h>
#include <robinhood/list.h>

#include "deduplicator.h"
#include "enricher.h"
#include "source.h"
#include "sink.h"

struct deduplicator_options {
    size_t batch_size;
};

static const size_t DEFAULT_BATCH_SIZE = 100;
static bool verbose = false;

static void
usage(void)
{
    const char *message =
        "usage: %s [OPTIONS] SOURCE DESTINATION\n"
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
        "    -c, --config PATH\n"
        "                    the path to a configuration file\n"
        "    --dry-run       displays the command after alias management\n"
        "    -d, --dump PATH\n"
        "                    the path to a file where the changelogs should be dumped,\n"
        "                    can only be used with a Lustre source. Use '-' for stdout.\n"
        "    -e, --enrich MOUNTPOINT\n"
        "                    enrich changelog records by querying MOUNTPOINT as needed\n"
        "                    MOUNTPOINT is a RobinHood URI (eg. rbh:lustre:/mnt/lustre)\n"
        "    -h, --help      print this message and exit\n"
        "    -m, --max NUMBER\n"
        "                    Set a maximum number of changelog to read\n"
        "    -r, --raw       do not enrich changelog records (default)\n"
        "    -v, --verbose   Set the verbose mode\n"
        "    -w, --nb-workers NUMBER\n"
        "                    number of workers to use to enrich and update the destination.\n"
        "\n"
        "Note that uploading raw records to a RobinHood backend will fail, they have to\n"
        "be enriched first.\n"
        "\n"
        "For Lustre sources, changelogs are not acknowledged by default. To\n"
        "enable this feature, you must specify in the Source URI the user\n"
        "with whom the acknowledge should be done, i.e.\n"
        "'src:lustre:lustre-MDT0000?ack-user=cl1'.\n";

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
source_from_uri(const char *uri, const char *dump_file, uint64_t max_changelog)
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

    (void) username;
    if (strcmp(raw_uri->path, "file") == 0) {
        source = source_from_file_uri(name, source_from_file);
    } else if (strcmp(raw_uri->path, "lustre") == 0) {
#ifdef HAVE_LUSTRE
        source = source_from_lustre_changelog(name, username, dump_file,
                                              max_changelog);
#else
        free(raw_uri);
        error(EX_USAGE, EINVAL, "MDT source is not available");
        __builtin_unreachable();
#endif
    } else if (strcmp(raw_uri->path, "hestia") == 0) {
        source = source_from_file_uri(name, source_from_hestia_file);
    }

    if (source) {
        free(raw_uri);
        return source;
    }

    error(EX_USAGE, 0, "%s: uri path not supported", raw_uri->path);
    __builtin_unreachable();
}

static struct source *
source_new(const char *arg, const char *dump_file, uint64_t max_changelog)
{
    if (strcmp(arg, "-") == 0)
        /* SOURCE is '-' (stdin) */
        return source_from_file(stdin);

    if (rbh_is_uri(arg))
        return source_from_uri(arg, dump_file, max_changelog);

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
        return (void *) sink_from_backend(rbh_backend_from_uri(uri, false));
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

static size_t nb_workers = 1;
static struct sink **sink;

static void __attribute__((destructor))
sink_exit(void)
{
    if (sink) {
        for (int i = 0; i < nb_workers; i++) {
            if (sink[i])
                sink_destroy(sink[i]);
        }
        free(sink);
    }
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
        uri_backend = rbh_backend_from_uri(uri, true);
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

static bool done_producing = false;
static bool skip_error = true;

struct rbh_node_iterator {
    struct rbh_iterator *enricher;
    struct rbh_list_node list;
};

/* Add an iterator to enrich to a consumer */
static void
add_iterators_to_consumer(struct rbh_list_node *list,
                          struct rbh_iterator *enricher)
{
    struct rbh_node_iterator *new_node = malloc(sizeof(*new_node));

    if (new_node == NULL)
        error(EXIT_FAILURE, ENOMEM, "malloc");

    new_node->enricher = enricher;

    rbh_list_add_tail(list, &new_node->list);
}

/* Retrieve an iterator from a consumer's list */
static struct rbh_node_iterator *
consumer_get_iterator(struct rbh_list_node *list)
{
    struct rbh_node_iterator *node;

    node = rbh_list_first(list, struct rbh_node_iterator, list);
    rbh_list_del(&node->list);

    return node;
}

struct consumer_info {
    struct timespec total_enrich;
    struct rbh_list_node *list;
    pthread_mutex_t mutex_list;
    pthread_cond_t signal_list;
    struct sink *sink;
};

/* Consumer loop */
void *
consumer_thread(void *arg) {
    struct consumer_info *cinfo = (struct consumer_info *) arg;
    struct rbh_node_iterator *node;
    struct timespec start, end;
    int rc;

    while (true) {
        pthread_mutex_lock(&cinfo->mutex_list);
        while (rbh_list_empty(cinfo->list) && !done_producing)
            pthread_cond_wait(&cinfo->signal_list, &cinfo->mutex_list);

        if (rbh_list_empty(cinfo->list) && done_producing) {
            pthread_mutex_unlock(&cinfo->mutex_list);
            errno = ENODATA;
            break;
        }

        node = consumer_get_iterator(cinfo->list);
        pthread_mutex_unlock(&cinfo->mutex_list);

        if (verbose) {
            rc = clock_gettime(CLOCK_REALTIME, &start);
            if (rc)
                error(EXIT_FAILURE, 0, "Unable to get start time");
        }

        if (sink_process(cinfo->sink, node->enricher)) {
            rbh_iter_destroy(node->enricher);
            free(node);
            break;
        }

        if (verbose) {
            rc = clock_gettime(CLOCK_REALTIME, &end);
            if (rc)
                error(EXIT_FAILURE, 0, "Unable to get end time");

            timespec_accumulate(&cinfo->total_enrich, start, end);
        }

        rbh_iter_destroy(node->enricher);
        free(node);
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

    return NULL;
}

static struct rbh_list_node *
init_consumer_list()
{
    struct rbh_list_node *list;

    list = malloc(sizeof(*list));
    if (list == NULL)
        return NULL;

    rbh_list_init(list);

    return list;
}

static void
setup_producer_consumers(struct rbh_mut_iterator **deduplicator,
                         struct deduplicator_options *dedup_opts,
                         pthread_t **consumers, struct consumer_info **cinfos)
{
    *deduplicator = deduplicator_new(dedup_opts->batch_size, source,
                                     nb_workers);
    if (deduplicator == NULL)
        error(EXIT_FAILURE, errno, "deduplicator_new");

    *consumers = malloc(nb_workers * sizeof(*consumers));
    if (consumers == NULL)
        error(EXIT_FAILURE, errno, "consumers malloc");

    *cinfos = malloc(nb_workers * sizeof(**cinfos));
    if (*cinfos == NULL)
        error(EXIT_FAILURE, errno, "cinfos malloc");

    for (int i = 0; i < nb_workers; i++) {
        struct consumer_info *cinfo = &(*cinfos)[i];

        memset(&cinfo->total_enrich, 0, sizeof(cinfo->total_enrich));
        pthread_mutex_init(&cinfo->mutex_list, NULL);
        pthread_cond_init(&cinfo->signal_list, NULL);
        cinfo->sink = sink[i];

        cinfo->list = init_consumer_list();
        if (cinfo->list == NULL)
            error(EXIT_FAILURE, errno, "init_consumer_list");

        if (pthread_create(&(*consumers)[i], NULL, consumer_thread, cinfo) != 0)
            error(EXIT_FAILURE, errno, "Failed to create the thread %d", i);
    }
}

/* Producer loop */
static void
producer_thread(struct rbh_mut_iterator *deduplicator,
                struct enrich_iter_builder *builder, bool allow_partials,
                struct consumer_info *cinfos, struct timespec *total_read)
{
    struct rbh_mut_iterator *fsevents;
    struct timespec start, end;
    struct dedup_iter *dedup;
    int rc;

    if (verbose) {
        rc = clock_gettime(CLOCK_REALTIME, &start);
        if (rc)
            error(EXIT_FAILURE, 0, "Unable to get start time");
    }

    for (fsevents = rbh_mut_iter_next(deduplicator); fsevents != NULL;
         fsevents = rbh_mut_iter_next(deduplicator)) {

        for (dedup = rbh_mut_iter_next(fsevents); dedup != NULL;
             dedup = rbh_mut_iter_next(fsevents)) {

            if (builder != NULL)
                dedup->iter = build_enrich_iter(builder, dedup->iter,
                                                skip_error);
            else if (!allow_partials)
                dedup->iter = iter_no_partial(dedup->iter);

            if (dedup->iter == NULL)
                error(EXIT_FAILURE, errno, "iter_enrich");

            pthread_mutex_lock(&cinfos[dedup->index].mutex_list);
            add_iterators_to_consumer(cinfos[dedup->index].list, dedup->iter);
            pthread_cond_signal(&cinfos[dedup->index].signal_list);
            pthread_mutex_unlock(&cinfos[dedup->index].mutex_list);
        }
        rbh_mut_iter_destroy(fsevents);
    }

    done_producing = true;

    if (verbose) {
        rc = clock_gettime(CLOCK_REALTIME, &end);
        if (rc)
            error(EXIT_FAILURE, 0, "Unable to get end time");

        timespec_accumulate(total_read, start, end);
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
}

static void
cleanup_producer_consumers(struct rbh_mut_iterator *deduplicator,
                           struct consumer_info *cinfos, pthread_t *consumers,
                           struct timespec *total_enrich)
{
    for (int i = 0; i < nb_workers; i++) {
        /* Wake up the the consumers */
        pthread_cond_signal(&cinfos[i].signal_list);
        pthread_join(consumers[i], NULL);

        *total_enrich = timespec_add(*total_enrich, cinfos->total_enrich);
        pthread_cond_destroy(&cinfos[i].signal_list);
        pthread_mutex_destroy(&cinfos[i].mutex_list);
        rbh_list_del(cinfos[i].list);
        free(cinfos[i].list);
    }

    free(cinfos);
    free(consumers);
    rbh_mut_iter_destroy(deduplicator);
}

static void
feed(struct sink **sink, struct source *source,
     struct enrich_iter_builder *builder, bool allow_partials,
     struct deduplicator_options *dedup_opts)
{
    struct rbh_mut_iterator *deduplicator = NULL;
    struct consumer_info *cinfos = NULL;
    struct timespec total_enrich = {0};
    struct timespec total_read = {0};
    pthread_t *consumers = NULL;

    /* Setup the producer and consumers */
    setup_producer_consumers(&deduplicator, dedup_opts, &consumers, &cinfos);

    /* Launch the producer loop */
    producer_thread(deduplicator, builder, allow_partials, cinfos, &total_read);

    /* Cleanup the producer and consumers */
    cleanup_producer_consumers(deduplicator, cinfos, consumers, &total_enrich);

    if (verbose) {
        printf("Total time elapsed to read changelogs and dedup:"
               "%ld.%09ld seconds\n", total_read.tv_sec, total_read.tv_nsec);
        printf("Total time elapsed to enrich and update mongo:"
               "%ld.%09ld seconds\n", total_enrich.tv_sec / nb_workers,
               total_enrich.tv_nsec / nb_workers);
    }
}

static int
insert_backend_source()
{
    const struct rbh_value_pair *pair;
    const struct rbh_value *sources;
    struct rbh_value_map *info_map;

    info_map = enrich_iter_builder_get_source_backends(enrich_builder);
    if (info_map == NULL) {
        fprintf(stderr, "Failed to retrieve source backends from enricher\n");
        return -1;
    }

    assert(info_map->count == 1);

    pair = &info_map->pairs[0];
    assert(strcmp(pair->key, "backend_source") == 0);
    sources = pair->value;

    if (sink_insert_source(sink[0], sources)) {
        fprintf(stderr, "Failed to set backend_info\n");
        return -1;
    }

    return 0;
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
            .name = "dry-run",
            .has_arg = no_argument,
            .val = 'x',
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
            .name = "max",
            .has_arg = required_argument,
            .val = 'm',
        },
        {
            .name = "no-skip",
            .val = 'n',
        },
        {
            .name = "nb-workers",
            .has_arg = required_argument,
            .val = 'w',
        },
        {
            .name = "raw",
            .val = 'r',
        },
        {
            .name = "verbose",
            .has_arg = no_argument,
            .val = 'v',
        },
        {}
    };
    struct deduplicator_options dedup_opts = {
        .batch_size = DEFAULT_BATCH_SIZE,
    };
    uint64_t max_changelog = 0;
    char *dump_file = NULL;
    int rc;
    char c;

    rc = rbh_config_from_args(argc - 1, argv + 1);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to load configuration file");

    rbh_apply_aliases(&argc, &argv);

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "b:c:d:e:hm:nrvw:", LONG_OPTIONS,
                            NULL)) != -1) {
        switch (c) {
        case 'b':
            if (str2uint64_t(optarg, &dedup_opts.batch_size))
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
        case 'm':
            if (str2uint64_t(optarg, &max_changelog))
                error(EXIT_FAILURE, 0, "'%s' is not an integer", optarg);
            break;
        case 'n':
            skip_error = false;
            break;
        case 'w':
            if (str2uint64_t(optarg, &nb_workers))
                error(EXIT_FAILURE, 0, "'%s' is not an integer", optarg);
            break;
        case 'r':
            /* Ignore errors on close */
            mount_fd_exit();
            mount_fd = -1;
            break;
        case 'x':
            rbh_display_resolved_argv(NULL, &argc, &argv);
            return EXIT_SUCCESS;
        case 'v':
            verbose = true;
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

    if (dump_file && strcmp(argv[optind + 1], dump_file) == 0)
        error(EX_USAGE, EINVAL,
              "Cannot output changelogs and fsevents both to stdout");

    source = source_new(argv[optind++], dump_file, max_changelog);
    sink = calloc(nb_workers, sizeof(*sink));
    if (sink == NULL)
        error(EXIT_FAILURE, errno, "calloc");

    for (int i = 0; i < nb_workers; i++)
        sink[i] = sink_new(argv[optind]);

    if (enrich_builder) {
        if (insert_backend_source() && errno != ENOTSUP)
            error(EX_USAGE, EINVAL,
                  "Failed to insert source backends in destination");
    }

    feed(sink, source, enrich_builder, strcmp(sink[0]->name, "backend"),
         &dedup_opts);

    rbh_config_free();

    return error_message_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
