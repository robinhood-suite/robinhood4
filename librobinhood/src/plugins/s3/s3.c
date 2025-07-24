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

#include "robinhood/backends/s3_mpi.h"
#include "robinhood/backends/s3.h"
#include "robinhood/config.h"
#include "robinhood/sstack.h"
#include "robinhood/statx.h"
#include "s3_internals.h"

#include "s3_wrapper.h"

/*----------------------------------------------------------------------------*
 |                                s3_iterator                                 |
 *----------------------------------------------------------------------------*/

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
s3_iterator_new(char* bucket_name)
{
    struct s3_iterator *s3_iter = NULL;

    s3_iter = malloc(sizeof(*s3_iter));
    if (s3_iter == NULL)
        return NULL;

    s3_iter->obj_data.list = NULL;
    if (bucket_name == NULL) {
        s3_get_bucket_list(&s3_iter->bkt_data.length, &s3_iter->bkt_data.list);
    } else {
        if (!s3_check_bucket(bucket_name))
            error(EXIT_FAILURE, ENODATA, "specified bucket does not exist");

        s3_iter->bkt_data.length = 1;
        s3_iter->bkt_data.list = malloc(sizeof(char*));
        s3_iter->bkt_data.list[0] = bucket_name;
    }

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

    /*--------------------------------------------------------------------*
     |                               branch                               |
     *--------------------------------------------------------------------*/

struct s3_branch_backend {
    struct s3_backend s3;
    char *bucket_name;
};


static struct rbh_backend *
s3_backend_branch(void *backend, const struct rbh_id *id,
                  const char *path);

static struct rbh_value_map *
s3_get_info(void *backend, int info_flags);

static void
s3_backend_destroy(void *backend);

static struct rbh_mut_iterator *
s3_branch_backend_filter(
    void *backend, const struct rbh_filter *filter,
    const struct rbh_filter_options *options,
    const struct rbh_filter_output *output)
{
    struct s3_branch_backend *branch = backend;
    struct s3_iterator *s3_iter;

    if (filter != NULL) {
        errno = ENOTSUP;
        return NULL;
    }

    if (options->skip > 0 || options->limit > 0 || options->sort.count > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    s3_iter = (struct s3_iterator *) branch->s3.iter_new(branch->bucket_name);
    if (s3_iter == NULL)
        return NULL;

    return &s3_iter->iterator;
}

static struct rbh_value_map *
s3_branch_get_info(void *backend, int info_flags)
{
    struct s3_branch_backend *branch = backend;

    return s3_get_info(&branch->s3, info_flags);
}

static const struct rbh_backend_operations S3_BRANCH_BACKEND_OPS = {
    .branch = s3_backend_branch,
    .filter = s3_branch_backend_filter,
    .get_info = s3_branch_get_info,
    .destroy = s3_backend_destroy,
};

static const struct rbh_backend S3_BRANCH_BACKEND = {
    .name = RBH_S3_BACKEND_NAME,
    .ops = &S3_BRANCH_BACKEND_OPS,
};

static struct rbh_backend *
s3_backend_branch(void *backend, const struct rbh_id *id,
                  const char *path)
{
    struct s3_branch_backend *branch;

    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    branch = malloc(sizeof(*branch));
    if (branch == NULL)
        return NULL;

    branch->bucket_name = strdup(path);
    branch->s3.backend = S3_BRANCH_BACKEND;
    branch->s3.iter_new = s3_iterator_new;

    return &branch->s3.backend;
}

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

    s3_iter = s3->iter_new(NULL);
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
}

void
rbh_s3_plugin_destroy()
{
    s3_destroy_api();
}

static const struct rbh_backend_operations S3_BACKEND_OPS = {
    .branch = s3_backend_branch,
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
    const char *type = uri->backend;
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

    if (!address && !region) {
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

    load_s3_extensions(&self->plugin, s3, type, config);

    return &s3->backend;
}
