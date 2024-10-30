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

bool
bson_append_aggregate_group_stage(bson_t *bson, const char *key,
                                  size_t key_length)
{
    bson_t document;

    if (!bson_append_document_begin(bson, key, key_length, &document))
        return false;

    return bson_append_document_end(bson, &document);
}
