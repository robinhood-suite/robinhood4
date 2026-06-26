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
#include <robinhood/backends/selinux.h>
#include <robinhood/config.h>


static bool
is_selinux_xattr_request(const struct rbh_value_pair *xattr)
{
    const struct rbh_value *value;

    if (xattr == NULL || xattr->key == NULL)
        return false;

    if (strcmp(xattr->key, "xattrs"))
        return false;

    value = xattr->value;
    if (value == NULL || value->type != RBH_VT_SEQUENCE)
        return false;

    for (size_t i = 0; i < value->sequence.count; i++) {
        const struct rbh_value *item = &value->sequence.values[i];

        if (item->type != RBH_VT_STRING)
            continue;

        if (!strcmp(item->string, "security.selinux"))
            return true;
    }

    return false;
}


static int
selinux_enrich_xattr(struct enricher *enricher,
                     struct rbh_posix_enrich_ctx *ctx,
                     const struct rbh_fsevent *original)
{
    struct rbh_value_pair *pairs;
    size_t n_xattrs;
    int count;
    int rc;

    rc = rbh_posix_enrich_open_by_id(ctx, enricher->mount_fd,
                                     &original->id);
    if (rc == -1)
        return -1;

    n_xattrs = enricher->fsevent.xattrs.count;
    pairs = enricher->pairs;

    count = rbh_backend_get_attribute(enricher->backend,
                                      RBH_XEF_SELINUX | RBH_XEF_ALL,
                                      ctx, &pairs[n_xattrs],
                                      enricher->pair_count - n_xattrs);

    if (count == -1)
        return -1;

    rc = convert_xattrs_with_operation(&pairs[n_xattrs], count, "set",
                                       ctx->values);
    if (rc)
        return rc;

    return count;
}

int
selinux_enrich_fsevent(struct enricher *enricher,
                       const struct enrich_request *req,
                       struct rbh_posix_enrich_ctx *ctx,
                       const struct rbh_fsevent *original)
{
    switch (req->type) {
    case ET_XATTR:
        if (!is_selinux_xattr_request(req->xattr)) {
            errno = ENOTSUP;
            return -1;
        }
        return selinux_enrich_xattr(enricher, ctx, original);

    case ET_STATX:
    case ET_INVAL:
        errno = ENOTSUP;
        return -1;
    }

    __builtin_unreachable();
}
