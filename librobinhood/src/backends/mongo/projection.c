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
 * { $project: { _id: 0, result: '$test'}}
 */
bool
bson_append_aggregate_projection_stage(bson_t *bson, const char *key,
                                       size_t key_length)
{
    bson_t document;

    return bson_append_document_begin(bson, key, key_length, &document)
        && BSON_APPEND_NULL(&document, "_id")
        && BSON_APPEND_UTF8(&document, "result", "$test")
        && bson_append_document_end(bson, &document);
}
