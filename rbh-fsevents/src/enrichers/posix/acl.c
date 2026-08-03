/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdbool.h>
#include <string.h>

#include "internals.h"
#include <backends/posix_extension.h>
#include <robinhood/backends/acl.h>
#include <robinhood/config.h>


static bool
is_acl_xattr_request(const struct rbh_value_pair *xattr)
{
    const struct rbh_value *value;

    if (strcmp(xattr->key, "xattrs"))
        return false;

    value = xattr->value;
    if (value == NULL || value->type != RBH_VT_SEQUENCE)
        return false;

    for (size_t i = 0; i < value->sequence.count; i++) {
        const struct rbh_value *item = &value->sequence.values[i];

        if (item->type != RBH_VT_STRING)
            continue;

        if (!strcmp(item->string, "system.posix_acl_access") ||
                !strcmp(item->string, "system.posix_acl_default"))
            return true;
    }
    return false;
}

static int
acl_enrich_xattr(struct enricher *enricher,
                 struct rbh_posix_enrich_ctx *ctx,
                 const struct rbh_fsevent *original)
{
    size_t n_xattrs = enricher->fsevent.xattrs.count;
    struct rbh_value_pair *pairs = enricher->pairs;
    int count;
    int rc;

    rc = rbh_posix_enrich_open_by_id(ctx, enricher->mount_fd,
                                     &original->id);
    if (rc == -1)
        return -1;

    count = rbh_backend_get_attribute(enricher->backend,
                                      RBH_AEF_ACL | RBH_AEF_ALL,
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
acl_enrich_fsevent(struct enricher *enricher,
                   const struct enrich_request *req,
                   struct rbh_posix_enrich_ctx *ctx,
                   const struct rbh_fsevent *original)
{
    switch (req->type) {
    case ET_XATTR:
        if (!is_acl_xattr_request(req->xattr)) {
            errno = ENOTSUP;
            return -1;
        }
        return acl_enrich_xattr(enricher, ctx, original);

    case ET_STATX:
        if (original->upsert.statx == NULL) {
            errno = ENOTSUP;
            return -1;
        }
        return acl_enrich_xattr(enricher, ctx, original);

    case ET_INVAL:
        errno = ENOTSUP;
        return -1;
    }

    __builtin_unreachable();
}
