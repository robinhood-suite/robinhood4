/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "robinhood/fsevent.h"
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

static struct rbh_fsevent *
fsevent_new(enum rbh_fsevent_type type, const struct rbh_id *id,
            size_t data_size, char **data)
{
    struct rbh_fsevent *fsevent;

    fsevent = malloc(sizeof(*fsevent) + id->size + data_size);
    if (fsevent == NULL)
        return NULL;
    fsevent->type = type;
    *data = (char *)fsevent + sizeof(*fsevent);

    memcpy(*data, id->data, id->size);
    fsevent->id.data = *data;
    fsevent->id.size = id->size;
    *data += id->size;

    return fsevent;
}

struct rbh_fsevent *
rbh_fsevent_upsert_new(const struct rbh_id *id, const struct statx *statxbuf,
                       const char *symlink)
{
    struct rbh_fsevent *fsevent;
    size_t symlink_length = 0;
    size_t size;
    char *data;

    if (symlink) {
        if (statxbuf && (statxbuf->stx_mask & STATX_TYPE)
                && !S_ISLNK(statxbuf->stx_mode)) {
            errno = EINVAL;
            return NULL;
        }
        symlink_length = strlen(symlink) + 1;
    }

    size = statxbuf ? sizeof(*statxbuf) : 0;
    size += symlink_length;

    fsevent = fsevent_new(RBH_FET_UPSERT, id, size, &data);
    if (fsevent == NULL)
        return NULL;

    if (statxbuf) {
        memcpy(data, statxbuf, sizeof(*statxbuf));
        fsevent->upsert.statx = (struct statx *)data;
        data += sizeof(*statxbuf);
    } else {
        fsevent->upsert.statx = NULL;
    }

    if (symlink) {
        memcpy(data, symlink, symlink_length);
        fsevent->upsert.symlink = data;
        data += sizeof(*symlink);
    } else {
        fsevent->upsert.symlink = NULL;
    }

    return fsevent;
}

static struct rbh_fsevent *
fsevent_link_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                 const char *name, bool link)
{
    struct rbh_fsevent *fsevent;
    struct rbh_id *tmp;
    size_t name_length;
    size_t size;
    char *data;

    name_length = strlen(name) + 1;
    size = sizeof(*parent_id) + parent_id->size + name_length;

    fsevent =
        fsevent_new(link ? RBH_FET_LINK : RBH_FET_UNLINK, id, size, &data);
    if (fsevent == NULL)
        return NULL;

    tmp = (struct rbh_id *)data;
    data += sizeof(*tmp);

    memcpy(data, parent_id->data, parent_id->size);
    tmp->data = data;
    tmp->size = parent_id->size;
    data += parent_id->size;

    fsevent->link.parent_id = tmp;

    memcpy(data, name, name_length);
    fsevent->link.name = data;
    data += name_length;

    return fsevent;
}

struct rbh_fsevent *
rbh_fsevent_link_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                     const char *name)
{
    return fsevent_link_new(id, parent_id, name, true);
}

struct rbh_fsevent *
rbh_fsevent_unlink_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                       const char *name)
{
    return fsevent_link_new(id, parent_id, name, false);
}

struct rbh_fsevent *
rbh_fsevent_delete_new(const struct rbh_id *id)
{
    char *data;

    return fsevent_new(RBH_FET_DELETE, id, 0, &data);
}
