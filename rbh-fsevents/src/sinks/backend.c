/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>
#include <errno.h>
#include <stdlib.h>

#include <robinhood/backend.h>
#include "robinhood/sstack.h"

#include "sink.h"

struct backend_sink {
    struct sink sink;
    struct rbh_backend *backend;
};

static int
backend_sink_process(void *_sink, struct rbh_iterator *fsevents)
{
    struct backend_sink *sink = _sink;

    return rbh_backend_update(sink->backend, fsevents) >= 0 ? 0 : -1;
}

static __thread struct rbh_sstack *metadata_sstack;

static void __attribute__ ((destructor))
destroy_metadata_sstack(void)
{
    if (metadata_sstack)
        rbh_sstack_destroy(metadata_sstack);
}

static int
backend_sink_insert_source(void *_sink, const struct rbh_value *backend_source)
{
    if (backend_source == NULL)
        return -1;

    struct backend_sink *sink = _sink;
    struct rbh_value_map *value_map;
    struct rbh_value_pair *pair;

    if (metadata_sstack == NULL) {
        metadata_sstack = rbh_sstack_new(sizeof(struct rbh_value_map));
        if (!metadata_sstack)
            return -1;
    }

    value_map = RBH_SSTACK_PUSH(metadata_sstack, NULL, sizeof(*value_map));
    pair = RBH_SSTACK_PUSH(metadata_sstack, NULL, sizeof(*pair));

    pair[0].key = "backend_source";
    pair[0].value = backend_source;

    value_map->pairs = pair;
    value_map->count = 1;

    return rbh_backend_insert_metadata(sink->backend, value_map, RBH_DT_INFO);
}

static void
backend_sink_destroy(void *_sink)
{
    struct backend_sink *sink = _sink;

    rbh_backend_destroy(sink->backend);
    free(sink);
}

static const struct sink_operations BACKEND_SINK_OPS = {
    .process = backend_sink_process,
    .insert_source = backend_sink_insert_source,
    .destroy = backend_sink_destroy,
};

static const struct sink BACKEND_SINK = {
    .name = "backend",
    .ops = &BACKEND_SINK_OPS,
};

struct sink *
sink_from_backend(struct rbh_backend *backend)
{
    struct backend_sink *sink;

    sink = malloc(sizeof(*sink));
    if (sink == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    sink->sink = BACKEND_SINK;
    sink->backend = backend;
    return &sink->sink;
}
