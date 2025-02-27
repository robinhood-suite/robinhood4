/* This file is part of RobinHood 4
 * Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "robinhood/backends/hestia.h"
#include "robinhood/sstack.h"
#include "robinhood/statx.h"

#include "hestia/hestia_iosea.h"

/*----------------------------------------------------------------------------*
 |                              hestia_iterator                               |
 *----------------------------------------------------------------------------*/

struct tier_objects {
    HestiaId *ids;
    size_t ids_length;
    size_t current_id; /* index of the object in 'ids' that will be managed in
                        * the next call to "hestia_iter_next". */
};

struct hestia_iterator {
    struct rbh_mut_iterator iterator;
    struct rbh_sstack *values;
    struct tier_objects *tiers;
    uint8_t *tier_ids;
    size_t tiers_length;
    size_t current_tier; /* index of the tier in "tiers" that will be managed in
                          * the next call to "hestia_iter_next". */
};

static HestiaId *
get_next_object(struct hestia_iterator *hestia_iter)
{
    struct tier_objects *tier = &hestia_iter->tiers[hestia_iter->current_tier];
    HestiaId *to_return;

    if (tier->current_id == tier->ids_length) {
        if (hestia_iter->current_tier + 1 == hestia_iter->tiers_length) {
            errno = ENODATA;
            return NULL;
        }

        hestia_iter->current_tier++;
        return get_next_object(hestia_iter);
    }

    to_return = &tier->ids[tier->current_id];

    tier->current_id++;

    return to_return;
}

static void
fill_statx(struct rbh_statx *statx, HestiaObject *obj)
{
    statx->stx_mask = RBH_STATX_SIZE | RBH_STATX_ATIME | RBH_STATX_BTIME |
                      RBH_STATX_CTIME | RBH_STATX_MTIME;

    statx->stx_size = obj->m_size;
    statx->stx_atime.tv_sec = obj->m_atime;
    statx->stx_atime.tv_nsec = 0;
    statx->stx_btime.tv_sec = obj->m_creation_time;
    statx->stx_btime.tv_nsec = 0;
    statx->stx_ctime.tv_sec = obj->m_ctime;
    statx->stx_ctime.tv_nsec = 0;
    statx->stx_mtime.tv_sec = obj->m_mtime;
    statx->stx_mtime.tv_nsec = 0;
}

static int
fill_path(char *path, struct rbh_value_pair **_pairs, struct rbh_sstack *values)
{
    struct rbh_value *path_value;
    struct rbh_value_pair *pair;

    path_value = rbh_sstack_push(values, NULL, sizeof(*path_value));
    if (path_value == NULL)
        return -1;

    path_value->type = RBH_VT_STRING;
    path_value->string = path;

    pair = rbh_sstack_push(values, NULL, sizeof(*pair));
    if (pair == NULL)
        return -1;

    pair->key = "path";
    pair->value = path_value;

    *_pairs = pair;

    return 0;
}

static void
create_tier_map(struct rbh_value *value, HestiaTierExtent *extent,
                struct rbh_sstack *sstack)
{
    struct rbh_value *extent_size = rbh_sstack_push(sstack, NULL,
                                                    sizeof(*extent_size));
    struct rbh_value_map *map = rbh_sstack_push(sstack, NULL, sizeof(*map));
    struct rbh_value *tier_extents = rbh_sstack_push(sstack, NULL,
                                                     sizeof(*tier_extents));
    struct rbh_value *tier_index = rbh_sstack_push(sstack, NULL,
                                                   sizeof(*tier_index));
    struct rbh_value_pair *pairs = rbh_sstack_push(sstack, NULL,
                                                   sizeof(*pairs) * 2);

    if (map == NULL || pairs == NULL || tier_index == NULL ||
        tier_extents == NULL || extent_size == NULL)
        error(EXIT_FAILURE, ENOMEM, "rbh_sstack_push in create_tier_map");

    extent_size->type = RBH_VT_UINT64;
    extent_size->uint64 = extent->m_size;

    tier_extents->type = RBH_VT_SEQUENCE;
    tier_extents->sequence.count = 1;
    tier_extents->sequence.values = extent_size;

    tier_index->type = RBH_VT_UINT32;
    tier_index->uint32 = extent->m_tier_index;

    pairs[0].key = "extents";
    pairs[0].value = tier_extents;

    pairs[1].key = "index";
    pairs[1].value = tier_index;

    map->count = 2;
    map->pairs = pairs;

    value->type = RBH_VT_MAP;
    value->map = *map;
}

static void
fill_tier_attributes(HestiaObject *obj, struct rbh_value_pair *pairs,
                     struct rbh_sstack *stack)
{
    struct rbh_value *tiers_sequence = rbh_sstack_push(stack, NULL,
                                                       sizeof(*tiers_sequence));
    struct rbh_value *tiers;

    if (tiers_sequence == NULL)
        error(EXIT_FAILURE, errno,
              "rbh_sstack_push on tiers_sequence in fill_tier_attributes");

    tiers_sequence->type = RBH_VT_SEQUENCE;
    tiers_sequence->sequence.count = obj->m_num_tier_extents;
    tiers = rbh_sstack_push(stack, NULL,
                            sizeof(*tiers) * tiers_sequence->sequence.count);
    if (tiers == NULL)
        error(EXIT_FAILURE, errno,
              "rbh_sstack_push on tiers in fill_tier_attributes");

    for (size_t i = 0; i < obj->m_num_tier_extents; ++i) {
        HestiaTierExtent *extent = &obj->m_tier_extents[i];

        create_tier_map(&tiers[i], extent, stack);
    }

    tiers_sequence->sequence.values = tiers;

    pairs[0].key = "tiers";
    pairs[0].value = tiers_sequence;
}

static void
fill_user_attributes(HestiaObject *obj, struct rbh_value_pair *pairs,
                     struct rbh_sstack *values)
{
    struct rbh_value *map_value = rbh_sstack_push(values, NULL,
                                                  sizeof(*map_value));
    struct rbh_value_pair *user_pairs = rbh_sstack_push(values, NULL,
                                                        sizeof(*user_pairs) *
                                                        obj->m_num_attrs);
    struct rbh_value_map map;

    if (map_value == NULL || user_pairs == NULL)
        error(EXIT_FAILURE, ENOMEM,
              "rbh_sstack_push in fill_user_attributes");

    for (size_t i = 0; i < obj->m_num_attrs; ++i) {
        HestiaKeyValuePair *attr = &obj->m_attrs[i];
        struct rbh_value *attr_value;

        attr_value = rbh_sstack_push(values, NULL, sizeof(*attr_value));
        if (attr_value == NULL)
            error(EXIT_FAILURE, ENOMEM,
                  "rbh_sstack_push on attr_value in fill_user_attributes");

        attr_value->type = RBH_VT_STRING;
        attr_value->string = rbh_sstack_push(values, attr->m_value,
                                             strlen(attr->m_value) + 1);
        if (attr_value->string == NULL)
            error(EXIT_FAILURE, ENOMEM,
                  "rbh_sstack_push on value string in fill_user_attributes");

        user_pairs[i].key = rbh_sstack_push(values, attr->m_key,
                                            strlen(attr->m_key) + 1);
        if (user_pairs[i].key == NULL)
            error(EXIT_FAILURE, ENOMEM,
                  "rbh_sstack_push on key pair in fill_user_attributes");

        user_pairs[i].value = attr_value;
    }

    map.count = obj->m_num_attrs;
    map.pairs = user_pairs;

    map_value->type = RBH_VT_MAP;
    map_value->map = map;

    pairs->key = "user_metadata";
    pairs->value = map_value;
}

static void *
hestia_iter_next(void *iterator)
{
    struct hestia_iterator *hestia_iter = iterator;
    struct rbh_fsentry *fsentry = NULL;
    struct rbh_value_pair *inode_pairs;
    struct rbh_value_map inode_xattrs;
    struct rbh_value_pair *ns_pairs;
    struct rbh_value_map ns_xattrs;
    struct rbh_statx statx = {0};
    struct rbh_id parent_id;
    HestiaObject obj = {0};
    struct rbh_id id;
    HestiaId *obj_id;
    char *name;
    int rc;

    obj_id = get_next_object(hestia_iter);
    if (obj_id == NULL)
        return NULL;

    /* Use the hestia_id of each file as rbh_id */
    id.data = rbh_sstack_push(hestia_iter->values, obj_id->m_uuid,
                              strlen(obj_id->m_uuid) + 1);
    if (id.data == NULL)
        return NULL;

    id.size = strlen(obj_id->m_uuid) + 1;

    /* All objects have no parent */
    parent_id.size = 0;

    rc = hestia_object_get_attrs(obj_id, &obj);
    if (rc)
        return NULL;

    fill_statx(&statx, &obj);

    // TODO: register the name of the object
    name = strdup(obj.m_uuid);
    if (name == NULL)
        return NULL;

    rc = fill_path(name, &ns_pairs, hestia_iter->values);
    if (rc)
        goto err;

    ns_xattrs.pairs = ns_pairs;
    ns_xattrs.count = 1;

    inode_pairs = rbh_sstack_push(hestia_iter->values, NULL,
                                  sizeof(*inode_pairs) * 2);
    if (inode_pairs == NULL)
        error(EXIT_FAILURE, errno, "rbh_sstack_push in fill_tier_attributes");

    fill_tier_attributes(&obj, &inode_pairs[0], hestia_iter->values);

    fill_user_attributes(&obj, &inode_pairs[1], hestia_iter->values);

    inode_xattrs.pairs = inode_pairs;
    inode_xattrs.count = 2;

    fsentry = rbh_fsentry_new(&id, &parent_id, name, &statx, &ns_xattrs,
                              &inode_xattrs, NULL);

err:
    free(name);
    return fsentry;
}

static void
hestia_iter_destroy(void *iterator)
{
    struct hestia_iterator *hestia_iter = iterator;

    rbh_sstack_destroy(hestia_iter->values);

    hestia_free_tier_ids(&hestia_iter->tier_ids);
    free(hestia_iter->tiers);
    free(hestia_iter);
}

static const struct rbh_mut_iterator_operations HESTIA_ITER_OPS = {
    .next = hestia_iter_next,
    .destroy = hestia_iter_destroy,
};

static const struct rbh_mut_iterator HESTIA_ITER = {
    .ops = &HESTIA_ITER_OPS,
};

struct hestia_iterator *
hestia_iterator_new()
{
    struct hestia_iterator *hestia_iter = NULL;
    struct hestia_id *ids = NULL;
    size_t tiers_len;
    int save_errno;
    uint8_t *tiers;
    int rc;

    hestia_iter = malloc(sizeof(*hestia_iter));
    if (hestia_iter == NULL)
        return NULL;

    rc = hestia_list_tiers(&tiers, &tiers_len);
    if (rc)
        goto err;

    hestia_iter->tiers_length = tiers_len;
    hestia_iter->tier_ids = tiers;
    hestia_iter->current_tier = 0;

    hestia_iter->tiers = malloc(sizeof(*hestia_iter->tiers) * tiers_len);
    if (hestia_iter->tiers == NULL)
        goto err_tiers;

    for (size_t i = 0; i < tiers_len; ++i) {
        HestiaId *object_ids;
        size_t num_ids;

        rc = hestia_object_list(tiers[i], &object_ids, &num_ids);
        if (rc)
            goto err_objects;

        hestia_iter->tiers[i].ids = object_ids;
        hestia_iter->tiers[i].ids_length = num_ids;
        hestia_iter->tiers[i].current_id = 0;
    }

    hestia_iter->iterator = HESTIA_ITER;

    hestia_iter->values = rbh_sstack_new(1 << 10);
    if (hestia_iter->values == NULL)
        goto err_objects;

    return hestia_iter;

err_objects:
    save_errno = errno;
    free(hestia_iter->tiers);
    errno = save_errno;

err_tiers:
    save_errno = errno;
    hestia_free_tier_ids(&tiers);
    errno = save_errno;

err:
    save_errno = errno;
    free(ids);
    free(hestia_iter);
    errno = save_errno;
    return NULL;
}

/*----------------------------------------------------------------------------*
 |                               hestia_backend                               |
 *----------------------------------------------------------------------------*/

struct hestia_backend {
    struct rbh_backend backend;
    struct hestia_iterator *(*iter_new)();
};

    /*--------------------------------------------------------------------*
     |                              filter()                              |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
hestia_backend_filter(void *backend, const struct rbh_filter *filter,
                      const struct rbh_filter_options *options)
{
    struct hestia_backend *hestia = backend;
    struct hestia_iterator *hestia_iter;

    if (filter != NULL) {
        errno = ENOTSUP;
        return NULL;
    }

    if (options->skip > 0 || options->limit > 0 || options->sort.count > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    hestia_iter = hestia->iter_new();
    if (hestia_iter == NULL)
        return NULL;

    return &hestia_iter->iterator;
}
    /*--------------------------------------------------------------------*
     |                             get_info()                             |
     *--------------------------------------------------------------------*/

void
hestia_backend_get_info(void *backend, enum rbh_get_info info)
{
    return;
}

    /*--------------------------------------------------------------------*
     |                             destroy()                              |
     *--------------------------------------------------------------------*/

static void
hestia_backend_destroy(void *backend)
{
    struct hestia_backend *hestia = backend;

    free(hestia);
    //hestia_finish();
}

static const struct rbh_backend_operations HESTIA_BACKEND_OPS = {
    .filter = hestia_backend_filter,
    .get_info = hestia_backend_get_info,
    .destroy = hestia_backend_destroy,
};

static const struct rbh_backend HESTIA_BACKEND = {
    .id = RBH_BI_HESTIA,
    .name = RBH_HESTIA_BACKEND_NAME,
    .ops = &HESTIA_BACKEND_OPS,
};

struct rbh_backend *
rbh_hestia_backend_new(__attribute__((unused)) const char *path,
                       __attribute__((unused)) struct rbh_config *config)
{
    struct hestia_backend *hestia;

    hestia = malloc(sizeof(*hestia));
    if (hestia == NULL)
        return NULL;

    hestia->iter_new = hestia_iterator_new;
    hestia->backend = HESTIA_BACKEND;

    hestia_initialize("/etc/hestia/hestiad.yaml", NULL, NULL);

    return &hestia->backend;
}
