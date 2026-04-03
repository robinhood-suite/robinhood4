/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "internals.h"

#include <backends/posix_extension.h>
#include <robinhood/backends/sparse.h>
#include <robinhood/config.h>

static int
sparse_enrich_statx(struct enricher *enricher,
                    struct rbh_posix_enrich_ctx *ctx,
                    const struct rbh_fsevent *original)
{
    struct rbh_value_pair *pairs;
    size_t n_xattrs;
    int count;
    int rc;

    n_xattrs = enricher->fsevent.xattrs.count;
    pairs = enricher->pairs;

    count = rbh_backend_get_attribute(enricher->backend,
                                      RBH_SEF_SPARSE | RBH_SEF_ALL,
                                      ctx, &pairs[n_xattrs],
                                      enricher->pair_count - n_xattrs);

    rc = convert_xattrs_with_operation(&pairs[n_xattrs], count, "set",
                                       ctx->values);
    if (rc)
        return rc;

    return count;
}

int
sparse_enrich_fsevent(struct enricher *enricher,
                      const struct enrich_request *req,
                      struct rbh_posix_enrich_ctx *ctx,
                      const struct rbh_fsevent *original)
{
    switch (req->type) {
    case ET_STATX:
        if (req->statx_mask & (RBH_STATX_BLOCKS | RBH_STATX_SIZE))
            return sparse_enrich_statx(enricher, ctx, original);

        errno = ENOTSUP;
        return -1;
    case ET_XATTR:
    case ET_INVAL:
        errno = ENOTSUP;
        return -1;
    }

    __builtin_unreachable();
}
