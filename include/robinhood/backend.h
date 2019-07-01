/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_BACKEND_H
#define ROBINHOOD_BACKEND_H

#include <errno.h>

#include <sys/types.h>

#include "robinhood/filter.h"
#include "robinhood/fsentry.h"
#include "robinhood/iterator.h"

struct rbh_backend {
    const char *name;
    const struct rbh_backend_operations *ops;
};

struct rbh_backend_operations {
    struct rbh_mut_iterator *(*filter_fsentries)(
            void *backend,
            const struct rbh_filter *filter,
            unsigned int fsentry_mask,
            unsigned int statx_mask
            );
    void (*destroy)(
            void *backend
            );
};

/**
 * Returns fsentries that match certain criteria
 *
 * @param backend       the backend from which to fetch fsentries
 * @param filter        a set of criteria that the returned fsentries must match
 * @param fsentry_mask  a bitmask of the fields to return for each fsentry
 * @param statx_mask    a bitmask of the fields to return in the statx field of
 *                      each fsentry (ignored if FP_STATX is not set in
 *                      \p fsentry_mask).
 *
 * @return              an iterator over mutable fsentries on success, NULL on
 *                      error and errno is set appropriately
 *
 * @error ENOTSUP       \p backend does not support filtering fsentries
 *
 * Backends may choose to return more (reps. less) fields in fsentries than is
 * required by \p fsentry_mask and \p statx_mask because it the extra
 * information comes at little to no cost (resp. the backend is missing this
 * information).
 *
 * It is the caller's responsability to check the corresponding masks in the
 * returned fsentries to know whether or not a given field is set and safe to
 * access.
 */
static inline struct rbh_mut_iterator *
rbh_backend_filter_fsentries(struct rbh_backend *backend,
                             const struct rbh_filter *filter,
                             unsigned int fsentry_mask,
                             unsigned int statx_mask)
{
    if (backend->ops->filter_fsentries == NULL) {
        errno = ENOTSUP;
        return NULL;
    }
    return backend->ops->filter_fsentries(backend, filter, fsentry_mask,
                                          statx_mask);
}

/**
 * Free resources associated to a struct rbh_backend
 *
 * @param backend   a pointer to the struct rbh_backend to reclaim
 */
static inline void
rbh_backend_destroy(struct rbh_backend *backend)
{
    return backend->ops->destroy(backend);
}

#endif
