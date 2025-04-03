/* SPDX-License-Identifer: LGPL-3.0-or-later */

#include "rbh_fsevent_utils.h"

#include <stddef.h>
#include <string.h>
#include <stdio.h>

const struct rbh_value *
rbh_map_find(const struct rbh_value_map *map, const char *key)
{
    for (size_t i = 0; i < map->count; i++)
        if (!strcmp(map->pairs[i].key, key))
            return map->pairs[i].value;

    return NULL;
}

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

static int
rbh_value_map_deep_copy(struct rbh_value_map *dest,
                        const struct rbh_value_map *src,
                        struct rbh_sstack *stack)
{
    struct rbh_value_pair *tmp;

    tmp = RBH_SSTACK_PUSH(stack, NULL, src->count * sizeof(*src->pairs));

    dest->count = src->count;
    dest->pairs = tmp;

    for (size_t i = 0; i < src->count; i++) {
        struct rbh_value_pair *pair = &tmp[i];
        struct rbh_value *value;
        int rc;

        pair->key = RBH_SSTACK_PUSH(stack, src->pairs[i].key,
                                    strlen(src->pairs[i].key) + 1);
        if (!pair->key)
            return -1;

        if (src->pairs[i].value == NULL) {
            pair->value = NULL;
            continue;
        }

        value = RBH_SSTACK_PUSH(stack, NULL, sizeof(*value));

        pair->value = value;
        rc = rbh_value_deep_copy(value, src->pairs[i].value, stack);
        if (rc)
            return -1;
    }

    return 0;
}

static int
rbh_sequence_deep_copy(struct rbh_value *dest,
                       const struct rbh_value *src,
                       struct rbh_sstack *stack)
{
    struct rbh_value *tmp;

    tmp = RBH_SSTACK_PUSH(stack, NULL, src->sequence.count * sizeof(*tmp));

    dest->sequence.count = src->sequence.count;
    dest->sequence.values = tmp;

    memset(tmp, 0, dest->sequence.count * sizeof(*tmp));

    for (size_t i = 0; i < src->sequence.count; i++) {
        int rc = rbh_value_deep_copy(&tmp[i],
                                     &src->sequence.values[i],
                                     stack);
        if (rc)
            return -1;
    }

    return 0;
}

int
rbh_value_deep_copy(struct rbh_value *dest, const struct rbh_value *src,
                    struct rbh_sstack *stack)
{
    if (!src)
        return 0;

    switch (src->type) {
    case RBH_VT_BOOLEAN:
    case RBH_VT_INT32:
    case RBH_VT_UINT32:
    case RBH_VT_INT64:
    case RBH_VT_UINT64:
        *dest = *src;
        return 0;
    case RBH_VT_STRING:
        dest->type = RBH_VT_STRING;
        dest->string = RBH_SSTACK_PUSH(stack, src->string,
                                       strlen(src->string) + 1);

        return 0;
    case RBH_VT_BINARY:
        dest->type = RBH_VT_BINARY;
        dest->binary.size = src->binary.size;
        dest->binary.data = RBH_SSTACK_PUSH(stack, src->binary.data,
                                            src->binary.size);

        return 0;
    case RBH_VT_REGEX:
        dest->type = RBH_VT_REGEX;
        dest->regex.options = src->regex.options;
        dest->regex.string = RBH_SSTACK_PUSH(stack, src->regex.string,
                                             strlen(src->regex.string) + 1);

        return 0;
    case RBH_VT_SEQUENCE:
        dest->type = RBH_VT_SEQUENCE;
        return rbh_sequence_deep_copy(dest, src, stack);
    case RBH_VT_MAP:
        dest->type = RBH_VT_MAP;
        return rbh_value_map_deep_copy(&dest->map, &src->map, stack);
    }

    return 0;
}

int
rbh_fsevent_deep_copy(struct rbh_fsevent *dst,
                      const struct rbh_fsevent *src,
                      struct rbh_sstack *stack)
{
    struct rbh_id *parent;
    int rc;

    memset(dst, 0, sizeof(*dst));

    dst->type = src->type;
    dst->id.size = src->id.size;
    dst->id.data = RBH_SSTACK_PUSH(stack, src->id.data, src->id.size);

    if (src->xattrs.count > 0) {
        rc = rbh_value_map_deep_copy(&dst->xattrs, &src->xattrs, stack);
        if (rc)
            return rc;
    }

    switch (src->type) {
    case RBH_FET_UPSERT:
        if (src->upsert.statx)
            dst->upsert.statx = RBH_SSTACK_PUSH(stack, src->upsert.statx,
                                                sizeof(*src->upsert.statx));

        if (src->upsert.symlink)
            dst->upsert.symlink = RBH_SSTACK_PUSH(
                stack, src->upsert.symlink, strlen(src->upsert.symlink) + 1
                );

        break;
    case RBH_FET_LINK:
    case RBH_FET_UNLINK:
        parent = RBH_SSTACK_PUSH(stack, NULL, sizeof(*parent));

        parent->size = src->link.parent_id->size;
        parent->data = RBH_SSTACK_PUSH(stack, src->link.parent_id->data,
                                       src->link.parent_id->size);

        dst->link.parent_id = parent;
        dst->link.name = RBH_SSTACK_PUSH(stack, src->link.name,
                                         strlen(src->link.name) + 1);

        break;
    case RBH_FET_XATTR:
        if (src->ns.parent_id) {
            parent = RBH_SSTACK_PUSH(stack, NULL, sizeof(*parent));

            parent->size = src->ns.parent_id->size;
            parent->data = RBH_SSTACK_PUSH(
                stack, src->ns.parent_id->data, src->ns.parent_id->size);

            dst->ns.parent_id = parent;
        }

        if (src->ns.name)
            dst->ns.name = RBH_SSTACK_PUSH(
                stack, src->ns.name, strlen(src->ns.name) + 1);

        break;
    case RBH_FET_DELETE:
        /* nothing to do here */
        break;
    }

    return 0;
}
