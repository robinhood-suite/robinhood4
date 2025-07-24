/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <errno.h>

#include "robinhood/config.h"
#include "robinhood/plugins/backend.h"
#include "robinhood/value.h"

#include "robinhood/backends/s3_extension.h"

static char *
config_iterator_key(const char *type)
{
    char *key;

    if (asprintf(&key, "backends/%s/iterator", type) == -1)
        return NULL;

    return key;
}

static int
load_iterator(const struct rbh_plugin *self,
              struct s3_backend *s3,
              const char *iterator,
              const char *type)
{
    const struct rbh_s3_extension *extension;

    extension = rbh_s3_load_extension(self, iterator);
    if (!extension) {
        rbh_backend_error_printf("failed to load iterator '%s' for backend '%s'",
                                 iterator, type);
        return -1;
    }

    s3->iter_new = extension->iter_new;

    return 0;
}

int
load_s3_extensions(const struct rbh_plugin *self,
                   struct s3_backend *s3,
                   const char *type,
                   struct rbh_config *config)
{
    struct rbh_value iterator;
    char *key;
    int rc;

    if (!config)
        return 0;

    key = config_iterator_key(type);
    rc = rbh_config_find(key, &iterator, RBH_VT_STRING);
    free(key);
    switch (rc) {
    case KPR_FOUND:
        if (load_iterator(self, s3, iterator.string, type) == -1)
            return -1;
        break;
    case KPR_NOT_FOUND:
        break;
    default:
        rbh_backend_error_printf("failed to retrieve 'iterator' key in configuration: %s",
                                 strerror(errno));
        return -1;
    }

    return 0;
}
