/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "internals.h"

#include <backends/posix_extension.h>
#include <robinhood/backends/retention.h>
#include <robinhood/config.h>

static __thread const char *retention_attribute = NULL;

static int
retention_enrich_statx(struct enricher *enricher,
                       struct rbh_posix_enrich_ctx *ctx,
                       const struct rbh_fsevent *original)
{
    struct rbh_value_pair *pairs;
    size_t n_xattrs;

    n_xattrs = enricher->fsevent.xattrs.count;
    pairs = enricher->pairs;

    return rbh_backend_get_attribute(enricher->backend,
                                     RBH_REF_RETENTION | RBH_REF_ALL,
                                     ctx,
                                     &pairs[n_xattrs],
                                     enricher->pair_count - n_xattrs);
}

static int
retention_enrich_xattr(struct enricher *enricher,
                       const struct rbh_value_pair *xattr,
                       struct rbh_posix_enrich_ctx *ctx,
                       const struct rbh_fsevent *original)
{
    struct rbh_value_pair *pairs;
    size_t n_xattrs;

    if (strcmp(xattr->key, retention_attribute)) {
        errno = ENOTSUP;
        return -1;
    }

    /* Decrement xattr count to replace the binary value of the xattr. This is
     * necessary since the key of the retention attribute is the same as the
     * name of the extended attribute. Not replacing the old value would result
     * in a duplicate key in the DB request that isn't allowed.
     */
    assert(enricher->fsevent.xattrs.count > 0);
    enricher->fsevent.xattrs.count--;
    n_xattrs = enricher->fsevent.xattrs.count;
    pairs = enricher->pairs;

    return rbh_backend_get_attribute(enricher->backend,
                                     RBH_REF_RETENTION | RBH_REF_ALL,
                                     ctx,
                                     &pairs[n_xattrs],
                                     enricher->pair_count - n_xattrs + 1);
}

int
retention_enrich_fsevent(struct enricher *enricher,
                         const struct enrich_request *req,
                         struct rbh_posix_enrich_ctx *ctx,
                         const struct rbh_fsevent *original)
{
    if (!retention_attribute)
        retention_attribute = rbh_config_get_string(XATTR_EXPIRES_KEY,
                                                    "user.expires");

    if (!retention_attribute) {
        errno = ENOTSUP;
        return -1;
    }

    switch (req->type) {
    case ET_STATX:
        if (req->statx_mask &
            (RBH_STATX_ATIME | RBH_STATX_MTIME | RBH_STATX_CTIME))
            return retention_enrich_statx(enricher, ctx, original);

        errno = ENOTSUP;
        return -1;
    case ET_XATTR:
        return retention_enrich_xattr(enricher, req->xattr, ctx, original);
    case ET_INVAL:
        errno = ENOTSUP;
        return -1;
    }

    __builtin_unreachable();
}
