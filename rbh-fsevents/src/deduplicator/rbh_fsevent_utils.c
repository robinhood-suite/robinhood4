/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "rbh_fsevent_utils.h"

#include <stddef.h>
#include <string.h>
#include <stdio.h>

const struct rbh_value *
rbh_fsevent_find_partial_xattr(const struct rbh_fsevent *fsevent,
                               const char *key)
{
    const struct rbh_value_map *rbh_fsevents_map;
    const struct rbh_value *xattr_sequence;

    rbh_fsevents_map = rbh_fsevent_find_fsevents_map(fsevent);
    if (!rbh_fsevents_map)
        return NULL;

    xattr_sequence = rbh_map_find(rbh_fsevents_map, "xattrs");
    if (!xattr_sequence)
        return NULL;

    assert(xattr_sequence->type == RBH_VT_SEQUENCE);
    for (size_t i = 0; i < xattr_sequence->sequence.count; i++) {
        const struct rbh_value *xattr = &xattr_sequence->sequence.values[i];

        assert(xattr->type == RBH_VT_STRING);
        if (!strcmp(key, xattr->string))
            return xattr;
    }

    return NULL;
}

const struct rbh_value_map *
rbh_fsevent_find_fsevents_map(const struct rbh_fsevent *fsevent)
{
    for (size_t i = 0; i < fsevent->xattrs.count; i++) {
        const struct rbh_value_pair *xattr = &fsevent->xattrs.pairs[i];

        if (!strcmp("rbh-fsevents", xattr->key)) {
            assert(xattr->value->type == RBH_VT_MAP);
            return &xattr->value->map;
        }
    }

    return NULL;
}

const struct rbh_value_pair *
rbh_fsevent_find_enrich_element(const struct rbh_fsevent *fsevent,
                                const char *key)
{
    const struct rbh_value_map *rbh_fsevents_map;

    rbh_fsevents_map = rbh_fsevent_find_fsevents_map(fsevent);
    if (!rbh_fsevents_map)
        return NULL;

    for (size_t i = 0; i < rbh_fsevents_map->count; i++) {
        const struct rbh_value_pair *enrich_elem;

        enrich_elem = &rbh_fsevents_map->pairs[i];

        if (!strcmp(key, enrich_elem->key))
            return enrich_elem;
    }

    return NULL;
}

const struct rbh_value_pair *
rbh_fsevent_find_xattr(const struct rbh_fsevent *fsevent,
                       const char *key)
{
    for (size_t i = 0; i < fsevent->xattrs.count; i++) {
        const struct rbh_value_pair *xattr = &fsevent->xattrs.pairs[i];

        if (!strcmp(key, xattr->key))
            return xattr;
    }

    return NULL;
}
