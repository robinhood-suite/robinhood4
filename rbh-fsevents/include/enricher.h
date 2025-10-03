/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ENRICHER_H
#define ENRICHER_H

#include <robinhood/backend.h>
#include <robinhood/iterator.h>

struct enrich_iter_builder;

struct enrich_iter_builder_operations {
    struct rbh_iterator *(*build_iter)(void *builder,
                                       struct rbh_iterator *fsevents,
                                       bool skip_error);
    struct rbh_value_map *(*get_source_backends)(void *builder);
    void (*destroy)(void *builder);
};

struct enrich_iter_builder {
    const char *name;
    const char *type;
    struct rbh_backend *backend;
    const struct enrich_iter_builder_operations *ops;
    int mount_fd;
    const char *mount_path;
};

static inline struct rbh_iterator *
build_enrich_iter(struct enrich_iter_builder *builder,
                  struct rbh_iterator *fsevents,
                  bool skip_error)
{
    return builder->ops->build_iter(builder, fsevents, skip_error);
}

static inline struct rbh_value_map *
enrich_iter_builder_get_source_backends(struct enrich_iter_builder *builder)
{
    if (builder->ops->get_source_backends)
        return builder->ops->get_source_backends(builder);

    errno = ENOTSUP;
    return NULL;
}

static inline void
enrich_iter_builder_destroy(struct enrich_iter_builder *builder)
{
    builder->ops->destroy(builder);
}

struct enrich_iter_builder *
enrich_iter_builder_from_backend(const char *type,
                                 struct rbh_backend *rbh_backend,
                                 const char *mount_path);

struct rbh_iterator *
iter_no_partial(struct rbh_iterator *fsevents);

#endif
