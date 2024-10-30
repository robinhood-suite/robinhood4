/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/statx.h"

#include "mongo.h"

/* The resulting bson will be as such:
 * { $group: {_id: '', test: { $sum: "$statx.size" }}}
 */
bool
bson_append_aggregate_group_stage(bson_t *bson, const char *key,
                                  size_t key_length)
{
    bson_t document;
    bson_t subdoc;

    return bson_append_document_begin(bson, key, key_length, &document)
        && BSON_APPEND_NULL(&document, "_id")
        && (BSON_APPEND_DOCUMENT_BEGIN(&document, "test", &subdoc)
            && BSON_APPEND_UTF8(&subdoc, "$sum", "$statx.size")
            && bson_append_document_end(&document, &subdoc))
        && bson_append_document_end(bson, &document);
}
