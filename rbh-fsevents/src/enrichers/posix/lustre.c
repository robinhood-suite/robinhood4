/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <lustre/lustreapi.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "internals.h"

#include <robinhood/backends/lustre.h>
#include <robinhood/backends/posix_extension.h>
#include <robinhood/backends/common.h>
#include <robinhood/utils.h>
#include <robinhood/backends/retention.h>

static int
enrich_path(const char *mount_path, const struct rbh_id *id, const char *name,
            struct rbh_sstack *xattrs_values, struct rbh_value **_value)
{
    const struct lu_fid *fid = rbh_lu_fid_from_id(id);
    size_t name_length = strlen(name);
    char fid_str[FID_LEN];
    long long recno = 0;
    size_t path_length;
    int linkno = 0;
    char *path;
    int rc;

    path = RBH_SSTACK_PUSH(xattrs_values, NULL, PATH_MAX);

    rc = sprintf(fid_str, DFID, PFID(fid));
    if (rc < 0)
        return -1;

    /* lustre path */
    // FIXME this should be moved to the lustre extension otherwise the support
    // of lustre in rbh-fsevents is ditermined at compile time not by the
    // presence of the lustre extension.
    path[0] = '/';
    rc = llapi_fid2path(mount_path, fid_str, path + 1,
                        PATH_MAX - name_length - 2, &recno, &linkno);
    if (rc) {
        errno = -rc;
        return -1;
    }

    /* If not called on the root, llapi_fid2path_at will return "path/to",
     * meaning we need to lead the path by a '/' and add one at its end before
     * appending the file name, thus having "/path/to/file".
     * But, if called on the root, it will return "/", thus having "///file" as
     * the file path; we need to truncate the first two slashes before appending
     * the file name.
     */
    if (path[1] == '/' && path[2] == 0)
        path[0] = 0;

    path_length = strlen(path);
    /* remove the extra space left in the sstack */
    rc = rbh_sstack_pop(xattrs_values,
                        PATH_MAX - (path_length + name_length + 2));
    if (rc)
        return -1;

    path[path_length] = '/';
    strcpy(&path[path_length + 1], name);

    *_value = RBH_SSTACK_PUSH(xattrs_values, NULL, sizeof(**_value));

    (*_value)->type = RBH_VT_STRING;
    (*_value)->string = path;

    return 1;
}

static int
lustre_enrich_xattr(struct enricher *enricher,
                    const struct rbh_value_pair *xattr,
                    struct rbh_posix_enrich_ctx *ctx,
                    const struct rbh_fsevent *original)
{
    static const int STATX_FLAGS = AT_STATX_FORCE_SYNC | AT_EMPTY_PATH
                                 | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW;
    struct rbh_value_pair *pairs;
    size_t n_xattrs;

    n_xattrs = enricher->fsevent.xattrs.count;
    pairs = enricher->pairs;

    if (!strcmp(xattr->key, "lustre")) {
        int rc;

        rc = rbh_posix_enrich_open_by_id(ctx, enricher->mount_fd,
                                         &original->id);
        if (rc == -1)
            return rc;

        rc = rbh_posix_enrich_statx(ctx, STATX_FLAGS, RBH_STATX_MODE,
                                    &enricher->statx);
        if (rc == -1)
            return rc;

        return rbh_backend_get_attribute(enricher->backend,
                                         RBH_LEF_LUSTRE | RBH_LEF_ALL_NOFID,
                                         ctx,
                                         &pairs[n_xattrs],
                                         enricher->pair_count - n_xattrs);

    } else if (!strcmp(xattr->key, "path")) {
        struct rbh_value *value;
        int size;

        size = enrich_path(enricher->mount_path, original->link.parent_id,
                           original->link.name, ctx->values, &value);
        if (size == -1)
            return -1;

        pairs[enricher->fsevent.xattrs.count].key = "path";
        pairs[enricher->fsevent.xattrs.count].value = value;

        return size;
    } else {
        errno = ENOTSUP;
        return -1;
    }

    return 0;
}

int
lustre_enrich_fsevent(struct enricher *enricher,
                      const struct enrich_request *req,
                      struct rbh_posix_enrich_ctx *ctx,
                      const struct rbh_fsevent *original)
{
    switch (req->type) {
    case ET_STATX:
        errno = ENOTSUP;
        return -1;
    case ET_XATTR:
        return lustre_enrich_xattr(enricher, req->xattr, ctx, original);
    case ET_INVAL:
        errno = ENOTSUP;
        return -1;
    }

    __builtin_unreachable();
}
