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

static bool
insert_rbh_filter_value(bson_t *bson, struct rbh_modifier_field *field)
{
    bson_t document;

    if (!BSON_APPEND_DOCUMENT_BEGIN(bson, "test", &document))
        return false;

    return bson_append_document_end(bson, &document);
}

/* The resulting bson will be as such:
 * { $group: {_id: '', test: { $sum: "$statx.size" }}}
 */
bool
bson_append_aggregate_group_stage(bson_t *bson, const char *key,
                                  size_t key_length,
                                  const struct rbh_group_fields *group)
{
    bson_t document;

    if (!(bson_append_document_begin(bson, key, key_length, &document) &&
          BSON_APPEND_INT32(&document, "_id", 0)))
        return false;

    if (group && group->count) {
        for (size_t i = 0; i < group->count; i++) {
            struct rbh_modifier_field *field = &group->fields[i];

            if (!insert_rbh_filter_value(&document, field))
                return false;
        }
    } else {
        for (size_t i = 0; i < group->map.count; i++) {
            const struct rbh_value_pair *pair = &group->map.pairs[i];

            if (!BSON_APPEND_RBH_VALUE(&document, pair->key, pair->value))
                return false;
        }
    }

    return bson_append_document_end(bson, &document);
}
