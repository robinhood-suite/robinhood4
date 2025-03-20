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

#include "s3_wrapper.h"

/*----------------------------------------------------------------------------*
 |                                s3_iterator                                 |
 *----------------------------------------------------------------------------*/

struct buckets_data{
    size_t list_length;
    size_t current_id;
    char **list;
};

struct objects_data{
    size_t list_length;
    size_t current_id;
    char **list;
};

struct s3_iterator {
    struct rbh_mut_iterator iterator;
    struct buckets_data bkt_data;
    struct objects_data obj_data;
};

void * //placeholder return type
get_next_object(struct s3_iterator *s3_iter)
{
    size_t *curr_bkt_id = &s3_iter->bkt_data.current_id;
    size_t *curr_obj_id = &s3_iter->obj_data.current_id;

    if (*curr_obj_id + 1 >= s3_iter->obj_data.list_length) {
        do {
            (*curr_bkt_id)++;
            if (*curr_bkt_id + 1 >= s3_iter->bkt_data.list_length) {
                errno = ENODATA;
                return NULL;
            }

            get_object_list(s3_iter->bkt_data.list[*curr_bkt_id],
                            &s3_iter->obj_data.list_length,
                            &s3_iter->obj_data.list);
        } while (s3_iter->obj_data.list_length == 0);

        *curr_obj_id = 0;
    }else{
        (*curr_obj_id)++;
    }
}

static void
s3_iter_destroy(void *iterator)
{
    struct s3_iterator *s3_iter = iterator;

    free(s3_iter->bkt_data.list);
    free(s3_iter->obj_data.list);
    free(s3_iter);
}

static const struct rbh_mut_iterator_operations S3_ITER_OPS = {
    //.next = s3_iter_next,
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

    get_bucket_list(&s3_iter->bkt_data.list_length, &s3_iter->bkt_data.list);
    if (s3_iter->bkt_data.list == NULL){
        free(s3_iter);
        return NULL;
    }

    get_object_list(s3_iter->bkt_data.list[0], &s3_iter->obj_data.list_length,
                    &s3_iter->obj_data.list);

    s3_iter->bkt_data.current_id = 0;
    s3_iter->obj_data.current_id = 0;
    s3_iter->iterator = S3_ITER;

    return s3_iter;
}

/*----------------------------------------------------------------------------*
 |                                  s3_backend                                |
 *----------------------------------------------------------------------------*/

struct s3_backend
{
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
rbh_s3_backend_new(__attribute__((unused)) const char *path,
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

    if (!address || !user || !password)
        return NULL;

    s3_init_api(address, user, password);
    s3->iter_new = s3_iterator_new;
    s3->backend = S3_BACKEND;

    return &s3->backend;
}
