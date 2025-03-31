/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
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
#include <stdbool.h>
#include <string.h>

#include "robinhood/backends/s3.h"
#include "robinhood/config.h"
#include "robinhood/sstack.h"
#include "robinhood/statx.h"

#include "s3_wrapper.h"

/*----------------------------------------------------------------------------*
 |                                s3_iterator                                 |
 *----------------------------------------------------------------------------*/

struct item_data {
    size_t length;
    size_t current_id;
    char **list;
};

struct s3_iterator {
    struct rbh_mut_iterator iterator;
    struct rbh_sstack *values;
    struct item_data bkt_data;
    struct item_data obj_data;
};

char *
get_next_object(struct s3_iterator *s3_iter)
{
    size_t *curr_bkt_id = &s3_iter->bkt_data.current_id;
    size_t *curr_obj_id = &s3_iter->obj_data.current_id;

    if (s3_iter->bkt_data.length == 0) {
        errno = ENODATA;
        return NULL;
    }

    if (*curr_obj_id == s3_iter->obj_data.length - 1) {
        do {
            (*curr_bkt_id)++;
            if (*curr_bkt_id >= s3_iter->bkt_data.length) {
                errno = ENODATA;
                return NULL;
            }

            s3_delete_list(s3_iter->obj_data.length, s3_iter->obj_data.list);
            s3_get_object_list(s3_iter->bkt_data.list[*curr_bkt_id],
                               &s3_iter->obj_data.length,
                               &s3_iter->obj_data.list);
        } while (s3_iter->obj_data.length == 0);

        *curr_obj_id = 0;
    } else {
        (*curr_obj_id)++;
    }
    return s3_iter->obj_data.list[*curr_obj_id];
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
fill_user_metadata(struct rbh_value_pair *pairs,
                   struct rbh_sstack *values)
{
    size_t custom_size = s3_get_custom_size();
    struct rbh_value *map_value = RBH_SSTACK_PUSH(values, NULL,
                                                  sizeof(*map_value));
    struct rbh_value_pair *user_pairs = RBH_SSTACK_PUSH(values, NULL,
                                                        sizeof(*user_pairs) *
                                                        custom_size);
    struct rbh_value_map map;

    if (map_value == NULL || user_pairs == NULL)
        error(EXIT_FAILURE, ENOMEM,
              "rbh_sstack_push in fill_user_metadata");

    for (size_t i = 0; i < custom_size; ++i) {
        struct rbh_value *attr_value;
        struct map_entry *entry;

        entry = s3_get_user_metadata_entry();

        attr_value = RBH_SSTACK_PUSH(values, NULL, sizeof(*attr_value));
        if (attr_value == NULL)
            error(EXIT_FAILURE, ENOMEM,
                  "rbh_sstack_push on attr_value in fill_user_metadata");

        attr_value->type = RBH_VT_STRING;
        attr_value->string = RBH_SSTACK_PUSH(values, entry->value,
                                             strlen(entry->value) + 1);
        if (attr_value->string == NULL)
            error(EXIT_FAILURE, ENOMEM,
                  "rbh_sstack_push on value string in fill_user_metadata");

        user_pairs[i].key = RBH_SSTACK_PUSH(values, entry->key,
                                            strlen(entry->key) + 1);
        if (user_pairs[i].key == NULL)
            error(EXIT_FAILURE, ENOMEM,
                  "rbh_sstack_push on key pair in fill_user_metadata");

        user_pairs[i].value = attr_value;
    }

    map.count = custom_size;
    map.pairs = user_pairs;

    map_value->type = RBH_VT_MAP;
    map_value->map = map;

    pairs->key = "user_metadata";
    pairs->value = map_value;
}

static void *
s3_iter_next(void *iterator)
{
    struct s3_iterator *s3_iter = iterator;
    struct rbh_fsentry *fsentry = NULL;
    struct rbh_value_pair *inode_pairs;
    struct rbh_value_map inode_xattrs;
    struct rbh_value_pair *ns_pairs;
    struct rbh_value_map ns_xattrs;
    struct rbh_statx statx = {0};
    struct rbh_id parent_id;
    char *current_bucket;
    char *current_object;
    struct rbh_id id;
    char *full_path;
    size_t length;
    int rc;

    current_object = get_next_object(s3_iter);
    if (current_object == NULL)
        return NULL;

    current_bucket = s3_iter->bkt_data.list[s3_iter->bkt_data.current_id];

    rc = s3_create_metadata(current_bucket, current_object);
    if (rc)
        return NULL;

    length = strlen(current_bucket) + strlen(current_object) + 2;
    full_path = malloc(length);
    if (full_path == NULL)
        return NULL;

    rc = snprintf(full_path, length, "%s/%s", current_bucket, current_object);
    if (rc == -1)
        goto err;

    id.data = RBH_SSTACK_PUSH(s3_iter->values, full_path,
                              strlen(full_path) + 1);
    id.size = strlen(full_path);

    parent_id.size = 0;

    statx.stx_mask = RBH_STATX_SIZE | RBH_STATX_MTIME;
    statx.stx_size = s3_get_size();
    statx.stx_mtime.tv_sec = s3_get_mtime();
    statx.stx_mtime.tv_nsec = 0;

    rc = fill_path(full_path, &ns_pairs, s3_iter->values);
    if (rc)
        goto err;

    ns_xattrs.pairs = ns_pairs;
    ns_xattrs.count = 1;

    inode_pairs = RBH_SSTACK_PUSH(s3_iter->values, NULL,
                                  sizeof(*inode_pairs));

    if (inode_pairs == NULL)
        error(EXIT_FAILURE, errno, "rbh_sstack_push error");

    fill_user_metadata(&inode_pairs[0], s3_iter->values);

    inode_xattrs.pairs = inode_pairs;
    inode_xattrs.count = 1;

    fsentry = rbh_fsentry_new(&id, &parent_id, current_object, &statx,
                              &ns_xattrs, &inode_xattrs, NULL);
err:
    free(full_path);
    return fsentry;
}

static void
s3_iter_destroy(void *iterator)
{
    struct s3_iterator *s3_iter = iterator;

    rbh_sstack_destroy(s3_iter->values);

    s3_delete_list(s3_iter->obj_data.length, s3_iter->obj_data.list);
    s3_delete_list(s3_iter->bkt_data.length, s3_iter->bkt_data.list);

    free(s3_iter);
}

static const struct rbh_mut_iterator_operations S3_ITER_OPS = {
    .next = s3_iter_next,
    .destroy = s3_iter_destroy,
};

static const struct rbh_mut_iterator S3_ITER = {
    .ops = &S3_ITER_OPS,
};

static struct s3_iterator *
s3_iterator_new()
{
    struct s3_iterator *s3_iter = NULL;

    s3_iter = malloc(sizeof(*s3_iter));
    if (s3_iter == NULL)
        return NULL;

    s3_get_bucket_list(&s3_iter->bkt_data.length, &s3_iter->bkt_data.list);

    if (s3_iter->bkt_data.length == -1) {
        free(s3_iter);
        return NULL;
    }

    if (s3_iter->bkt_data.length > 0)
        s3_get_object_list(s3_iter->bkt_data.list[0], &s3_iter->obj_data.length,
                           &s3_iter->obj_data.list);

    s3_iter->bkt_data.current_id = 0;
    s3_iter->obj_data.current_id = -1;
    s3_iter->iterator = S3_ITER;

    s3_iter->values = rbh_sstack_new(1 << 10);
    if (s3_iter->values == NULL)
        return NULL;

    return s3_iter;
}

/*----------------------------------------------------------------------------*
 |                                  s3_backend                                |
 *----------------------------------------------------------------------------*/

struct s3_backend {
    struct rbh_backend backend;
    struct s3_iterator *(*iter_new)();
};

    /*--------------------------------------------------------------------*
     |                              filter()                              |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
s3_backend_filter(void *backend, const struct rbh_filter *filter,
                  const struct rbh_filter_options *options,
                  const struct rbh_filter_output *output)
{
    struct s3_backend *s3 = backend;
    struct s3_iterator *s3_iter;

    if (filter != NULL) {
        errno = ENOTSUP;
        return NULL;
    }

    if (options->skip > 0 || options->limit > 0 || options->sort.count > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    s3_iter = s3->iter_new();
    if (s3_iter == NULL)
        return NULL;

    return &s3_iter->iterator;
}

    /*--------------------------------------------------------------------*
     |                             destroy()                              |
     *--------------------------------------------------------------------*/

static void
s3_backend_destroy(void *backend)
{
    struct s3_backend *s3 = backend;
    free(s3);
    s3_destroy_api();
}

static const struct rbh_backend_operations S3_BACKEND_OPS = {
    .filter = s3_backend_filter,
    .destroy = s3_backend_destroy,
};

static const struct rbh_backend S3_BACKEND = {
    .id = RBH_BI_S3,
    .name = RBH_S3_BACKEND_NAME,
    .ops = &S3_BACKEND_OPS,
};

struct rbh_backend *
rbh_s3_backend_new(__attribute__((unused))
                   const struct rbh_backend_plugin *self,
                   __attribute__((unused)) const char *type,
                   __attribute__((unused)) const char *path,
                   struct rbh_config *config)
{
    struct rbh_value value = { 0 };
    struct s3_backend *s3;
    const char *password;
    const char *address;
    const char *user;

    s3 = malloc(sizeof(*s3));
    if (s3 == NULL)
        return NULL;

    load_rbh_config(config);

    rbh_config_find("RBH_S3/S3_PASSWORD", &value, RBH_VT_STRING);
    password = value.string;
    rbh_config_find("RBH_S3/S3_ADDRESS", &value, RBH_VT_STRING);
    address = value.string;
    rbh_config_find("RBH_S3/S3_USER", &value, RBH_VT_STRING);
    user = value.string;

    if (!address || !user || !password){
        rbh_backend_error_printf("could not retrieve the s3 setup variables from
                                 the config file");
        return NULL;
    }

    s3_init_api(address, user, password);
    s3->iter_new = s3_iterator_new;
    s3->backend = S3_BACKEND;

    return &s3->backend;
}
