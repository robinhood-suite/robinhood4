/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ENRICHER_H
#define ENRICHER_H

#include <robinhood/backend.h>
#include <robinhood/iterator.h>

struct enrich_iter_builder;

struct enrich_iter_builder_operations {
    struct rbh_iterator *(*build_iter)(void *builder,
                                       struct rbh_iterator *fsevents);
    void (*destroy)(void *builder);
};

struct enrich_iter_builder {
    const char *name;
    struct rbh_backend *backend;
    const struct enrich_iter_builder_operations *ops;
    int mount_fd;
};

static inline struct rbh_iterator *
enrich_iter_builder_build_iter(struct enrich_iter_builder *builder,
                               struct rbh_iterator *fsevents)
{
    return builder->ops->build_iter(builder, fsevents);
}

static inline void
enrich_iter_builder_destroy(struct enrich_iter_builder *builder)
{
    builder->ops->destroy(builder);
}

struct enrich_iter_builder *
enrich_iter_builder_from_backend(struct rbh_backend *rbh_backend,
                                 const char *mount_path);

struct rbh_iterator *
iter_no_partial(struct rbh_iterator *fsevents);

#endif
