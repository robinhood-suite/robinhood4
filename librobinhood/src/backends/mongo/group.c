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
                                  size_t key_length,
                                  const struct rbh_filter_group *group)
{
    bson_t document;
    bson_t subdoc;

    if (!(bson_append_document_begin(bson, key, key_length, &document) &&
          BSON_APPEND_INT32(&document, "_id", 0)))
        return false;

    if (!BSON_APPEND_DOCUMENT_BEGIN(&document, "test", &subdoc))
        return false;

    if (group == NULL || group->map.count == 0) {
        if (!BSON_APPEND_UTF8(&subdoc, "$sum", "$statx.size"))
            return false;
    } else {
        for (size_t i = 0; i < group->map.count; i++) {
            const struct rbh_value_pair *pair = &group->map.pairs[i];

            if (!BSON_APPEND_RBH_VALUE(&subdoc, pair->key, pair->value))
                return false;
        }
    }

    return bson_append_document_end(&document, &subdoc)
        && bson_append_document_end(bson, &document);
}
