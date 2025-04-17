/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef RBH_FSEVENTS_SINK_H
#define RBH_FSEVENTS_SINK_H

#include <stdio.h>

#include <robinhood/backend.h>
#include <robinhood/iterator.h>

struct sink;

struct sink_operations {
    int (*process)(void *sink, struct rbh_iterator *fsevents);
    int (*insert_source)(void *sink, const struct rbh_value *backend_source);
    void (*destroy)(void *sink);
};

struct sink {
    const char *name;
    const struct sink_operations *ops;
};

static inline int
sink_process(struct sink *sink, struct rbh_iterator *fsevents)
{
    return sink->ops->process(sink, fsevents);
}

static inline int
sink_insert_source(struct sink *sink, const struct rbh_value *backend_source)
{
    if (sink->ops->insert_source)
        return sink->ops->insert_source(sink, backend_source);

    errno = ENOTSUP;
    return 1;
}

static inline void
sink_destroy(struct sink *sink)
{
    return sink->ops->destroy(sink);
}

struct sink *
sink_from_backend(struct rbh_backend *rbh_backend);

struct sink *
sink_from_file(FILE *file);

#endif
