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
    int64_t length;
    int64_t current_id;
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
    int64_t *curr_bkt_id = &s3_iter->bkt_data.current_id;
    int64_t *curr_obj_id = &s3_iter->obj_data.current_id;

    if (s3_iter->bkt_data.length == 0) {
        errno = ENODATA;
        return NULL;
    }

    if (*curr_obj_id == s3_iter->obj_data.length - 1 ||
        s3_iter->obj_data.length == 0) {
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

    for (size_t i = 0; i < custom_size; ++i) {
        struct rbh_value *attr_value;
        struct map_entry *entry;

        entry = s3_get_user_metadata_entry();

        attr_value = RBH_SSTACK_PUSH(values, NULL, sizeof(*attr_value));

        attr_value->type = RBH_VT_STRING;
        attr_value->string = RBH_SSTACK_PUSH(values, entry->value,
                                             strlen(entry->value) + 1);

        user_pairs[i].key = RBH_SSTACK_PUSH(values, entry->key,
                                            strlen(entry->key) + 1);
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

    if (s3_get_custom_size() > 0) {
        fill_user_metadata(&inode_pairs[0], s3_iter->values);

        inode_xattrs.pairs = inode_pairs;
        inode_xattrs.count = 1;

        fsentry = rbh_fsentry_new(&id, &parent_id, current_object, &statx,
                                  &ns_xattrs, &inode_xattrs, NULL);
    } else {
        fsentry = rbh_fsentry_new(&id, &parent_id, current_object, &statx,
                                  &ns_xattrs, NULL, NULL);
    }

err:
    free(full_path);
    return fsentry;
}

static void
s3_iter_destroy(void *iterator)
{
    struct s3_iterator *s3_iter = iterator;

    rbh_sstack_destroy(s3_iter->values);
    if (s3_iter->obj_data.list != NULL)
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

    s3_iter->obj_data.list = NULL;
    s3_get_bucket_list(&s3_iter->bkt_data.length, &s3_iter->bkt_data.list);

    if (s3_iter->bkt_data.length == -1) {
        free(s3_iter);
        return NULL;
    }

    if (s3_iter->bkt_data.length > 0)
        s3_get_object_list(s3_iter->bkt_data.list[0], &s3_iter->obj_data.length,
                           &s3_iter->obj_data.list);

    s3_iter->obj_data.current_id = -1;
    s3_iter->bkt_data.current_id = 0;
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
     |                         get_info()                                 |
     *--------------------------------------------------------------------*/

#define MIN_VALUES_SSTACK_ALLOC (1 << 6)
static __thread struct rbh_sstack *info_sstack;

static void __attribute__((destructor))
destroy_info_sstack(void)
{
    if (info_sstack)
        rbh_sstack_destroy(info_sstack);
}

static const struct rbh_value RBH_S3_SOURCE_TYPE = {
    .type = RBH_VT_STRING,
    .string = "plugin",
};

static const struct rbh_value RBH_S3_SOURCE_NAME = {
    .type = RBH_VT_STRING,
    .string = "s3",
};

static const struct rbh_value_pair RBH_S3_SOURCE_PAIR[] = {
    { .key = "type", &RBH_S3_SOURCE_TYPE },
    { .key = "plugin", &RBH_S3_SOURCE_NAME },
};

static const struct rbh_value RBH_S3_SOURCE_MAP = {
    .type = RBH_VT_MAP,
    .map = {
        .pairs = RBH_S3_SOURCE_PAIR,
        .count = sizeof(RBH_S3_SOURCE_PAIR) / sizeof(RBH_S3_SOURCE_PAIR[0]),
    },
};

static const struct rbh_value RBH_S3_BACKEND_SEQUENCE = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .values = &RBH_S3_SOURCE_MAP,
        .count = 1,
    },
};

static struct rbh_value_map *
s3_get_info(__attribute__((unused)) void *backend, int info_flags)
{
    struct rbh_value_map *map_value;
    struct rbh_value_pair *pairs;
    int tmp_flags = info_flags;
    int count = 0;
    int idx = 0;

    while (tmp_flags) {
        count += tmp_flags & 1;
        tmp_flags >>= 1;
    }

    if (info_sstack == NULL) {
        info_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                     (sizeof(struct rbh_value_map *)));
        if (!info_sstack)
            goto out;
    }

    pairs = RBH_SSTACK_PUSH(info_sstack, NULL, count * sizeof(*pairs));
    if (!pairs)
        goto out;

    map_value = RBH_SSTACK_PUSH(info_sstack, NULL, sizeof(*map_value));
    if (!map_value)
        goto out;

    if (info_flags & RBH_INFO_BACKEND_SOURCE) {
        pairs[idx].key = "backend_source";
        pairs[idx++].value = &RBH_S3_BACKEND_SEQUENCE;
    }

    map_value->pairs = pairs;
    map_value->count = idx;

    return map_value;

out:
    errno = EINVAL;
    return NULL;
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
    .get_info = s3_get_info,
    .destroy = s3_backend_destroy,
};

static const struct rbh_backend S3_BACKEND = {
    .id = RBH_BI_S3,
    .name = RBH_S3_BACKEND_NAME,
    .ops = &S3_BACKEND_OPS,
};

static const char *
get_config_var(char *key)
{
    struct rbh_value value = { 0 };
    enum key_parse_result rc;

    rc = rbh_config_find(key, &value, RBH_VT_STRING);
    if (rc == KPR_ERROR)
        return NULL;

    return rc == KPR_FOUND ? value.string : NULL;
}

struct rbh_backend *
rbh_s3_backend_new(__attribute__((unused))
                   const struct rbh_backend_plugin *self,
                   const struct rbh_uri *uri,
                   struct rbh_config *config,
                   bool read_only)
{
    size_t host_len, port_len;
    struct s3_backend *s3;
    const char *crt_path;
    const char *password;
    const char *address;
    const char *region;
    uint64_t temp_port;
    const char *user;
    char buffer[255];
    char port[16];

    s3 = malloc(sizeof(*s3));
    if (s3 == NULL)
        return NULL;

    rbh_config_load(config);

    crt_path = get_config_var("s3/crt_path");
    region = get_config_var("s3/region");

    if (uri->authority) {
        password = uri->authority->password;
        if (!strcmp(password, ""))
            password = get_config_var("s3/password");

        user = uri->authority->username;
        if (!strcmp(user, ""))
            user = get_config_var("s3/user");

        if (uri->authority->port == 0)
            //Default port is 80 in HTTP and 443 in HTTPS
            temp_port = (crt_path != NULL ? 443 : 80);
        else
            temp_port = uri->authority->port;

        snprintf(port, sizeof(port), "%ld", temp_port);
        host_len = strlen(uri->authority->host);
        port_len = strlen(port);
        memcpy(buffer, uri->authority->host, host_len);
        memcpy(buffer + host_len, ":", 1);
        memcpy(buffer + host_len + 1, port, port_len);
        buffer[host_len + 1 + port_len] = '\0';
        address = buffer;
    } else {
        address = get_config_var("s3/address");
        user = get_config_var("s3/user");
        password = get_config_var("s3/password");
    }

    if (!address && region == NULL) {
        rbh_backend_error_printf("could not retrieve the address or region "
                                 "from the config file or the URI");
        return NULL;
    }

    if (!user || !password) {
        rbh_backend_error_printf("could not retrieve the user and password "
                                 "from the config file or the URI");
        return NULL;
    }

    s3_init_api(address, user, password, crt_path, region);
    s3->iter_new = s3_iterator_new;
    s3->backend = S3_BACKEND;

    return &s3->backend;
}
