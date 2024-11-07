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
                                       size_t key_length,
                                       const struct rbh_filter_output *output)
{
    bson_t document;

    if (!(bson_append_document_begin(bson, key, key_length, &document) &&
          BSON_APPEND_INT32(&document, "_id", 0)))
        return false;

    if (output->type != RBH_FOT_MAP || output->map.count == 0) {
        if (!BSON_APPEND_UTF8(&document, "result", "$test"))
            return false;
    } else {
        for (size_t i = 0; i < output->map.count; i++) {
            const struct rbh_value_pair *pair = &output->map.pairs[i];

            if (!BSON_APPEND_RBH_VALUE(&document, pair->key, pair->value))
                return false;
        }
    }

    return bson_append_document_end(bson, &document);
}
