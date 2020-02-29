/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/stat.h>

#include "robinhood/fsevent.h"
#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

#include "mongo.h"

/*----------------------------------------------------------------------------*
 |                        bson_selector_from_fsevent()                        |
 *----------------------------------------------------------------------------*/

static bool
bson_append_rbh_id_filter(bson_t *bson, const char *key, size_t key_length,
                          const struct rbh_id *id)
{
    return _bson_append_binary(bson, key, key_length, BSON_SUBTYPE_BINARY,
                               id->data, id->size)
        && (id->size != 0 || BSON_APPEND_INT32(bson, "type", BSON_TYPE_NULL));
}

#define BSON_APPEND_RBH_ID_FILTER(bson, key, id) \
    bson_append_rbh_id_filter(bson, key, strlen(key), id)

bson_t *
bson_selector_from_fsevent(const struct rbh_fsevent *fsevent)
{
    bson_t *selector = bson_new();

    if (BSON_APPEND_RBH_ID_FILTER(selector, MFF_ID, &fsevent->id))
        return selector;

    bson_destroy(selector);
    errno = ENOBUFS;
    return NULL;
}

/*----------------------------------------------------------------------------*
 |                         bson_update_from_fsevent()                         |
 *----------------------------------------------------------------------------*/

static bson_t *
bson_from_upsert(const struct statx *statxbuf, const char *symlink)
{
    bson_t *bson = bson_new();
    bson_t document;

    if (BSON_APPEND_DOCUMENT_BEGIN(bson, "$set", &document)
     && (statxbuf ? BSON_APPEND_STATX(&document, MFF_STATX, statxbuf) : true)
     && (symlink ? BSON_APPEND_UTF8(&document, MFF_SYMLINK, symlink) : true)
     && bson_append_document_end(bson, &document))
        return bson;

    bson_destroy(bson);
    errno = ENOBUFS;
    return NULL;
}

static bson_t *
bson_from_link(const struct rbh_id *parent_id, const char *name)
{
    bson_t *bson = bson_new();
    bson_t document;
    bson_t subdoc;

    if (BSON_APPEND_DOCUMENT_BEGIN(bson, "$addToSet", &document)
     && BSON_APPEND_DOCUMENT_BEGIN(&document, MFF_NAMESPACE, &subdoc)
     && BSON_APPEND_RBH_ID_FILTER(&subdoc, MFF_PARENT_ID, parent_id)
     && BSON_APPEND_UTF8(&subdoc, MFF_NAME, name)
     && bson_append_document_end(&document, &subdoc)
     && bson_append_document_end(bson, &document))
        return bson;

    bson_destroy(bson);
    errno = ENOBUFS;
    return NULL;
}

static bson_t *
bson_from_unlink(const struct rbh_id *parent_id, const char *name)
{
    bson_t *bson = bson_new();
    bson_t document;
    bson_t subdoc;

    if (BSON_APPEND_DOCUMENT_BEGIN(bson, "$pull", &document)
     && BSON_APPEND_DOCUMENT_BEGIN(&document, MFF_NAMESPACE, &subdoc)
     && BSON_APPEND_RBH_ID_FILTER(&subdoc, MFF_PARENT_ID, parent_id)
     && BSON_APPEND_UTF8(&subdoc, MFF_NAME, name)
     && bson_append_document_end(&document, &subdoc)
     && bson_append_document_end(bson, &document))
        return bson;

    bson_destroy(bson);
    errno = ENOBUFS;
    return NULL;
}

bson_t *
bson_update_from_fsevent(const struct rbh_fsevent *fsevent)
{
    switch (fsevent->type) {
    case RBH_FET_UPSERT:
        return bson_from_upsert(fsevent->upsert.statx, fsevent->upsert.symlink);
    case RBH_FET_LINK:
        return bson_from_link(fsevent->link.parent_id, fsevent->link.name);
    case RBH_FET_UNLINK:
        return bson_from_unlink(fsevent->link.parent_id, fsevent->link.name);
    default:
        errno = EINVAL;
        return NULL;
    }
}
