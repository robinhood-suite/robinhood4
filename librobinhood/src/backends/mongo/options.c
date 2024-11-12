/* This file is part of RobinHood 4
 * Copyright (C) 2020 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/statx.h"

#include "mongo.h"

/*----------------------------------------------------------------------------*
 |                       bson_append_rbh_filter_sorts()                       |
 *----------------------------------------------------------------------------*/

#define XATTR_ONSTACK_LENGTH 128

bool
bson_append_rbh_filter_sorts(bson_t *bson, const char *key, size_t key_length,
                             const struct rbh_filter_sort *items, size_t count)
{
    bson_t document;

    if (!bson_append_document_begin(bson, key, key_length, &document))
        return false;

    for (size_t i = 0; i < count; i++) {
        char onstack[XATTR_ONSTACK_LENGTH];
        char *buffer = onstack;
        bool success;

        key = field2str(&items[i].field, &buffer, sizeof(onstack));
        if (key == NULL)
            return false;

        success = BSON_APPEND_INT32(&document, key,
                                    items[i].ascending ? 1 : -1);
        if (buffer != onstack)
            free(buffer);
        if (!success)
            return false;
    }

    return bson_append_document_end(bson, &document);
}
