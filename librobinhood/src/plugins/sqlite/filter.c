/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static void *
sqlite_iter_next(void *iterator)
{
    errno = ENODATA;
    return NULL;
}

static void
sqlite_iter_destroy(void *iterator)
{
    struct sqlite_iterator *iter = iterator;

    free(iter);
}

static const struct rbh_mut_iterator_operations SQLITE_ITER_OPS = {
    .next    = sqlite_iter_next,
    .destroy = sqlite_iter_destroy,
};

static const struct rbh_mut_iterator SQLITE_ITER = {
    .ops = &SQLITE_ITER_OPS,
};

static struct sqlite_iterator *
sqlite_iterator_new(void)
{
    struct sqlite_iterator *iter;

    iter = malloc(sizeof(*iter));
    if (!iter)
        return NULL;

    iter->iter = SQLITE_ITER;

    return iter;
}

struct rbh_mut_iterator *
sqlite_backend_filter(void *backend, const struct rbh_filter *filter,
                      const struct rbh_filter_options *options,
                      const struct rbh_filter_output *output)
{
    struct sqlite_iterator *iter = sqlite_iterator_new();

    if (!iter)
        return NULL;

    return &iter->iter;
}
