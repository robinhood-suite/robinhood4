/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "robinhood/fsevent.h"
#include "robinhood/statx.h"

#include "utils.h"
#include "value.h"

static int
fsevent_copy(struct rbh_fsevent *dest, const struct rbh_fsevent *src,
             char **data_, size_t *data_size)
{
    size_t size = *data_size;
    char *data = *data_;
    struct rbh_id *id;
    size_t length;
    int rc;

    /* dest->type */
    dest->type = src->type;

    /* dest->id */
    rc = rbh_id_copy(&dest->id, &src->id, &data, &size);
    assert(rc == 0);

    /* dest->xattrs */
    rc = value_map_copy(&dest->xattrs, &src->xattrs, &data, &size);
    assert(rc == 0);

    switch (src->type) {
    case RBH_FET_UPSERT: /* dest->upsert */
        /* dest->upsert.statx */
        if (src->upsert.statx) {
            struct rbh_statx *tmp;

            tmp = aligned_memalloc(alignof(*tmp), sizeof(*tmp), &data, &size);
            assert(tmp);
            *tmp = *src->upsert.statx;
            dest->upsert.statx = tmp;
        } else {
            dest->upsert.statx = NULL;
        }

        /* dest->upsert.symlink */
        if (src->upsert.symlink) {
            length = strlen(src->upsert.symlink) + 1;
            assert(size >= length);

            dest->upsert.symlink = data;
            data = mempcpy(data, src->upsert.symlink, length);
            size -= length;
        } else {
            dest->upsert.symlink = NULL;
        }
        break;
    case RBH_FET_XATTR: /* dest->ns */
        if (src->ns.parent_id == NULL) {
            assert(src->ns.name == NULL);
            dest->ns.parent_id = NULL;
            dest->ns.name = NULL;
            break;
        }
        assert(src->ns.name);
        __attribute__((fallthrough));
    case RBH_FET_LINK:
    case RBH_FET_UNLINK: /* dest->link */
        /* dest->link.parent_id */
        id = aligned_memalloc(alignof(*id), sizeof(*id), &data, &size);
        assert(id);
        rc = rbh_id_copy(id, src->link.parent_id, &data, &size);
        assert(rc == 0);
        dest->link.parent_id = id;

        /* dest->link.name */
        length = strlen(src->link.name) + 1;
        assert(size >= length);

        dest->link.name = data;
        data = mempcpy(data, src->link.name, length);
        size -= length;
        break;
    case RBH_FET_DELETE:
        break;
    }

    *data_size = size;
    *data_ = data;
    return 0;
}

static size_t __attribute__((pure))
fsevent_data_size(const struct rbh_fsevent *fsevent)
{
    size_t size = 0;

    /* fsevent->id */
    size += fsevent->id.size;

    /* fsevent->xattrs */
    size = sizealign(size, alignof(*fsevent->xattrs.pairs));
    size += value_map_data_size(&fsevent->xattrs);

    /* fsevent->{upsert,link,ns} */
    switch (fsevent->type) {
    case RBH_FET_UPSERT: /* fsevent->upsert */
        if (fsevent->upsert.statx) {
            size = sizealign(size, alignof(*fsevent->upsert.statx));
            size += sizeof(*fsevent->upsert.statx);
        }
        if (fsevent->upsert.symlink)
            size += strlen(fsevent->upsert.symlink) + 1;
        break;
    case RBH_FET_XATTR: /* fsevent->ns */
        if (fsevent->ns.parent_id == NULL) {
            assert(fsevent->ns.name == NULL);
            break;
        }
        assert(fsevent->ns.name);
        __attribute__((fallthrough));
    case RBH_FET_LINK:
    case RBH_FET_UNLINK /* fsevent->link */:
        size = sizealign(size, alignof(*fsevent->link.parent_id));
        size += sizeof(*fsevent->link.parent_id);
        size += fsevent->link.parent_id->size;
        size += strlen(fsevent->link.name) + 1;
        break;
    case RBH_FET_DELETE:
        break;
    }

    return size;
}

static struct rbh_fsevent *
fsevent_clone(const struct rbh_fsevent *fsevent)
{
    struct rbh_fsevent *clone;
    size_t size;
    char *data;
    int rc;

    size = fsevent_data_size(fsevent);
    clone = malloc(sizeof(*clone) + size);
    if (clone == NULL)
        return NULL;
    data = (char *)clone + sizeof(*clone);

    rc = fsevent_copy(clone, fsevent, &data, &size);
    assert(rc == 0);

    return clone;
}

struct rbh_fsevent *
rbh_fsevent_upsert_new(const struct rbh_id *id,
                       const struct rbh_value_map *xattrs,
                       const struct rbh_statx *statxbuf, const char *symlink)
{
    const struct rbh_fsevent upsert = {
        .type = RBH_FET_UPSERT,
        .id = {
            .data = id->data,
            .size = id->size,
        },
        .xattrs = {
            .pairs = xattrs ? xattrs->pairs : NULL,
            .count = xattrs ? xattrs->count : 0,
        },
        .upsert = {
            .statx = statxbuf,
            .symlink = symlink,
        },
    };

    if (symlink && statxbuf && !S_ISLNK(statxbuf->stx_mode)) {
        errno = EINVAL;
        return NULL;
    }

    return fsevent_clone(&upsert);
}

struct rbh_fsevent *
rbh_fsevent_link_new(const struct rbh_id *id,
                     const struct rbh_value_map *xattrs,
                     const struct rbh_id *parent_id, const char *name)
{
    const struct rbh_fsevent link = {
        .type = RBH_FET_LINK,
        .id = {
            .data = id->data,
            .size = id->size,
        },
        .xattrs = {
            .pairs = xattrs ? xattrs->pairs : NULL,
            .count = xattrs ? xattrs->count : 0,
        },
        .link = {
            .parent_id = parent_id,
            .name = name,
        },
    };

    if (parent_id == NULL || name == NULL) {
        errno = EINVAL;
        return NULL;
    }

    return fsevent_clone(&link);
}

struct rbh_fsevent *
rbh_fsevent_unlink_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                       const char *name)
{
    const struct rbh_fsevent unlink = {
        .type = RBH_FET_UNLINK,
        .id = {
            .data = id->data,
            .size = id->size,
        },
        .xattrs.count = 0,
        .link = {
            .parent_id = parent_id,
            .name = name,
        },
    };

    if (parent_id == NULL || name == NULL) {
        errno = EINVAL;
        return NULL;
    }

    return fsevent_clone(&unlink);
}

struct rbh_fsevent *
rbh_fsevent_delete_new(const struct rbh_id *id)
{
    const struct rbh_fsevent delete = {
        .type = RBH_FET_DELETE,
        .id = {
            .data = id->data,
            .size = id->size,
        },
        .xattrs.count = 0,
    };

    return fsevent_clone(&delete);
}

struct rbh_fsevent *
rbh_fsevent_xattr_new(const struct rbh_id *id,
                      const struct rbh_value_map *xattrs)
{
    const struct rbh_fsevent xattr = {
        .type = RBH_FET_XATTR,
        .id = {
            .data = id->data,
            .size = id->size,
        },
        .xattrs = {
            .pairs = xattrs->pairs,
            .count = xattrs->count,
        },
        .ns = {
            .parent_id = NULL,
            .name = NULL,
        },
    };

    return fsevent_clone(&xattr);
}

struct rbh_fsevent *
rbh_fsevent_ns_xattr_new(const struct rbh_id *id,
                         const struct rbh_value_map *xattrs,
                         const struct rbh_id *parent_id, const char *name)
{
    const struct rbh_fsevent ns_xattr = {
        .type = RBH_FET_XATTR,
        .id = {
            .data = id->data,
            .size = id->size,
        },
        .xattrs = {
            .pairs = xattrs->pairs,
            .count = xattrs->count,
        },
        .ns = {
            .parent_id = parent_id,
            .name = name,
        },
    };

    if (parent_id == NULL || name == NULL) {
        errno = EINVAL;
        return NULL;
    }

    return fsevent_clone(&ns_xattr);
}

const char *
rbh_fsevent_path(const struct rbh_fsevent *fsevent)
{
    const struct rbh_value_map *xattrs = &fsevent->xattrs;

    for (size_t i = 0; i < xattrs->count; i++) {
        const struct rbh_value_pair *xattr = &xattrs->pairs[i];

        if (strcmp(xattr->key, "path"))
            continue;

        if (xattr->value->type != RBH_VT_STRING) {
            errno = EFAULT;
            return NULL;
        }

        return xattr->value->string;
    }

    errno = ENODATA;
    return NULL;
}
