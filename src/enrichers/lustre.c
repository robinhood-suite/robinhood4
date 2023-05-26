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

#include <robinhood.h>

#include "enricher.h"
#include "internals.h"

#define MIN_XATTR_VALUES_ALLOC (1 << 14)
static __thread struct rbh_sstack *xattrs_values;

static void __attribute__((destructor))
exit_xattrs_values(void)
{
    if (xattrs_values)
        rbh_sstack_destroy(xattrs_values);
}

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

    path = rbh_sstack_push(xattrs_values, NULL, PATH_MAX);
    if (path == NULL)
        return -1;

    rc = sprintf(fid_str, DFID, PFID(fid));
    if (rc < 0)
        return -1;

    /* lustre path */
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

    *_value = rbh_sstack_push(xattrs_values, NULL, sizeof(**_value));
    if (*_value == NULL)
        return -1;

    (*_value)->type = RBH_VT_STRING;
    (*_value)->string = path;

    return 1;
}

static int
enrich_lustre(struct rbh_backend *backend, int mount_fd,
              const struct rbh_id *id, struct rbh_sstack *xattrs_values,
              struct rbh_value_pair *pair)
{
    static const int STATX_FLAGS = AT_STATX_FORCE_SYNC | AT_EMPTY_PATH
                                 | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW;
    struct {
        int fd;
        uint16_t mode;
        struct rbh_sstack *values;
    } arg;
    struct rbh_statx statxbuf;
    int save_errno;
    int size;
    int rc;

    arg.fd = open_by_id(mount_fd, id, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (arg.fd < 0 && errno == ELOOP)
        /* If the file to open is a symlink, reopen it with O_PATH set */
        arg.fd = open_by_id(mount_fd, id,
                            O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_PATH);
    if (arg.fd < 0)
        return -1;

    rc = rbh_statx(arg.fd, "", STATX_FLAGS, RBH_STATX_MODE, &statxbuf);
    if (rc == -1) {
        save_errno = errno;
        close(arg.fd);
        errno = save_errno;
        return -1;
    }

    arg.mode = statxbuf.stx_mode;
    arg.values = xattrs_values;

    size = rbh_backend_get_attribute(backend, "lustre", &arg, pair);

    save_errno = errno;
    close(arg.fd);
    errno = save_errno;

    return size;
}

static int
lustre_enrich(struct enricher *enricher, const struct rbh_value_pair *attr,
              const struct rbh_fsevent *original)
{
    struct rbh_value_pair *pairs = enricher->pairs;
    int size;

    if (xattrs_values == NULL) {
        xattrs_values = rbh_sstack_new(MIN_XATTR_VALUES_ALLOC
                                     * sizeof(struct rbh_value *));
        if (xattrs_values == NULL)
            return -1;
    }

    if (strcmp(attr->key, "path") == 0) {
        struct rbh_value *value;

        size = enrich_path(enricher->mount_path, original->link.parent_id,
                           original->link.name, xattrs_values, &value);
        if (size == -1)
            return -1;

        pairs[enricher->fsevent.xattrs.count].key = "path";
        pairs[enricher->fsevent.xattrs.count].value = value;
        enricher->fsevent.xattrs.count += size;

        return size;
    }

    if (strcmp(attr->key, "lustre") == 0) {
        size = enrich_lustre(enricher->backend, enricher->mount_fd,
                             &original->id, xattrs_values,
                             &pairs[enricher->fsevent.xattrs.count]);
        if (size == -1)
            return -1;

        enricher->fsevent.xattrs.count += size;

        return size;
    }

    return posix_enrich(attr, &pairs, &enricher->pair_count, &enricher->fsevent,
                        original, enricher->mount_fd, &enricher->statx,
                        enricher->symlink);
}

static int
enrich(struct enricher *enricher, const struct rbh_fsevent *original)
{
    struct rbh_fsevent *enriched = &enricher->fsevent;
    struct rbh_value_pair *pairs = enricher->pairs;
    size_t pair_count = enricher->pair_count;

    *enriched = *original;
    enriched->xattrs.count = 0;

    for (size_t i = 0; i < original->xattrs.count; i++) {
        const struct rbh_value_pair *pair = &original->xattrs.pairs[i];
        const struct rbh_value_map *partials;

        if (strcmp(pair->key, "rbh-fsevents")) {
            if (enriched->xattrs.count + 1 >= pair_count) {
                void *tmp;

                tmp = reallocarray(pairs, pair_count << 1, sizeof(*pairs));
                if (tmp == NULL)
                    return -1;
                enricher->pairs = pairs = tmp;
                enricher->pair_count = pair_count = pair_count << 1;
            }
            pairs[enriched->xattrs.count++] = original->xattrs.pairs[i];
            continue;
        }

        if (pair->value == NULL || pair->value->type != RBH_VT_MAP) {
            errno = EINVAL;
            return -1;
        }
        partials = &pair->value->map;

        for (size_t i = 0; i < partials->count; i++) {
            int rc;

            rc = lustre_enrich(enricher, &partials->pairs[i], original);
            if (rc == -1)
                return -1;
        }

    }
    enriched->xattrs.pairs = pairs;

    return 0;
}

static const void *
lustre_enricher_iter_next(void *iterator)
{
    struct enricher *enricher = iterator;
    const void *fsevent;

    fsevent = rbh_iter_next(enricher->fsevents);
    if (fsevent == NULL)
        return NULL;

    if (enrich(enricher, fsevent))
        return NULL;

    return &enricher->fsevent;
}

static const struct rbh_iterator_operations LUSTRE_ENRICHER_ITER_OPS = {
    .next = lustre_enricher_iter_next,
    .destroy = posix_enricher_iter_destroy,
};

static struct rbh_iterator *
lustre_iter_enrich(struct rbh_backend *backend, struct rbh_iterator *fsevents,
                   int mount_fd, const char *mount_path)
{
    struct rbh_iterator *iter = posix_iter_enrich(fsevents, mount_fd,
                                                  mount_path);
    struct enricher *enricher = (struct enricher *)iter;

    if (iter == NULL)
        return NULL;

    enricher->backend = backend;
    enricher->iterator.ops = &LUSTRE_ENRICHER_ITER_OPS;

    return iter;
}

/*----------------------------------------------------------------------------*
 *                           lustre_backend_enrich                            *
 *----------------------------------------------------------------------------*/

static struct rbh_iterator *
lustre_enrich_iter_builder_build_iter(void *_builder,
                                      struct rbh_iterator *fsevents)
{
    struct enrich_iter_builder *builder = _builder;

    return lustre_iter_enrich(builder->backend, fsevents, builder->mount_fd,
                              builder->mount_path);
}

static const struct enrich_iter_builder_operations
LUSTRE_ENRICH_ITER_BUILDER_OPS = {
    .build_iter = lustre_enrich_iter_builder_build_iter,
    .destroy = posix_enrich_iter_builder_destroy,
};

const struct enrich_iter_builder LUSTRE_ENRICH_ITER_BUILDER = {
    .name = "lustre",
    .ops = &LUSTRE_ENRICH_ITER_BUILDER_OPS,
};
