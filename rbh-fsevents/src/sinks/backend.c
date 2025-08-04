/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>
#include <errno.h>
#include <stdlib.h>

#include "robinhood/backend.h"
#include "robinhood/sstack.h"
#include "robinhood/value.h"

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

static int
backend_sink_insert_metadata_info(void *_sink, const char *key,
                                  const struct rbh_value *value)
{
    struct backend_sink *sink = _sink;
    struct rbh_value_pair pair = {
        .key = key,
        .value = value
    };
    struct rbh_value_map value_map = {
        .pairs = &pair,
        .count = 1,
    };

    return rbh_backend_insert_metadata(sink->backend, &value_map, RBH_DT_INFO);
}

static int
backend_sink_insert_source(void *_sink, const struct rbh_value *backend_source)
{
    return backend_sink_insert_metadata_info(_sink, "backend_source",
                                             backend_source);
}

static int
backend_sink_insert_mountpoint(void *_sink, const struct rbh_value *mountpoint)
{
    return backend_sink_insert_metadata_info(_sink, "mountpoint",
                                             mountpoint);
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
    .insert_mountpoint = backend_sink_insert_mountpoint,
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
