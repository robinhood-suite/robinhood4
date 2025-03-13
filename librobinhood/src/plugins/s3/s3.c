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
 |                                  s3_backend                                |
 *----------------------------------------------------------------------------*/

struct s3_backend {
    struct rbh_backend backend;
};

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

    if (!address || !user || !password)
        return NULL;

    s3_init_api(address, user, password);
    s3->backend = S3_BACKEND;

    return &s3->backend;
}
