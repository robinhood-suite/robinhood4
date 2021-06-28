/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>
#include <errno.h>
#include <stdlib.h>

#include <robinhood/backend.h>

#include "sink.h"

struct backend_sink {
    struct sink sink;
    struct rbh_backend *backend;
};

static int
backend_sink_process(void *_sink, struct rbh_iterator *fsevents)
{
    struct backend_sink *sink = _sink;

    return rbh_backend_update(sink->backend, fsevents) >= 0;
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
