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

#include "robinhood/backends/posix_extension.h"

static char *
config_iterator_key(struct rbh_config *config, const char *type)
{
    char *key;

    if (asprintf(&key, "backends/%s/iterator", type) == -1)
        return NULL;

    return key;
}

static int
load_iterator(const struct rbh_plugin *self,
              struct posix_backend *posix,
              const char *iterator,
              const char *type)
{
    const struct rbh_posix_extension *extension;

    if (!strcmp(iterator, "fts"))
        return 0;

    extension = rbh_posix_load_extension(self, iterator);
    if (!extension) {
        rbh_backend_error_printf("failed to load iterator '%s' for backend '%s'",
                                 iterator, type);
        return -1;
    }

    posix->iter_new = extension->iter_new;

    return 0;
}

static char *
config_enrichers_key(struct rbh_config *config, const char *type)
{
    char *key;

    if (asprintf(&key, "backends/%s/enrichers", type) == -1)
        return NULL;

    return key;
}

static int
load_enrichers(const struct rbh_plugin *self,
               struct posix_backend *posix,
               const struct rbh_value *enrichers,
               const char *type)
{
    size_t count = enrichers->sequence.count + 1; /* NULL terminated */
    const struct rbh_posix_extension *extension;

    /* if we arrive here, we have at least one enricher to load */
    assert(count >= 2);

    posix->enrichers = malloc(sizeof(*posix->enrichers) * count);

    for (size_t i = 0; i < enrichers->sequence.count; i++) {
        const struct rbh_value *value = &enrichers->sequence.values[i];

        assert(value->type == RBH_VT_STRING);

        extension = rbh_posix_load_extension(self, value->string);
        if (!extension) {
            rbh_backend_error_printf("failed to load extension '%s' for backend '%s'",
                                     value->string, type);
            return -1;
        }

        posix->enrichers[i] = extension;
    }
    posix->enrichers[count - 1] = NULL;

    return 0;
}

int
rbh_posix_enrichers_list(struct rbh_config *config, const char *type,
                         struct rbh_value *enrichers)
{
    char *key;
    int rc;

    key = config_enrichers_key(config, type);
    rc = rbh_config_find(key, enrichers, RBH_VT_SEQUENCE);
    free(key);

    return rc;
}

int
load_posix_extensions(const struct rbh_plugin *self,
                      struct posix_backend *posix,
                      const char *type,
                      struct rbh_config *config)
{
    struct rbh_value enrichers;
    struct rbh_value iterator;
    char *key;
    int rc;

    if (!config)
        return 0;

    key = config_iterator_key(config, type);
    rc = rbh_config_find(key, &iterator, RBH_VT_STRING);
    free(key);
    switch (rc) {
    case KPR_FOUND:
        if (load_iterator(self, posix, iterator.string, type) == -1)
            return -1;
        break;
    case KPR_NOT_FOUND:
        break;
    default:
        rbh_backend_error_printf("failed to retrieve 'iterator' key in configuration: %s",
                                 strerror(errno));
        return -1;
    }

    rc = rbh_posix_enrichers_list(config, type, &enrichers);
    switch (rc) {
    case KPR_FOUND:
        if (load_enrichers(self, posix, &enrichers, type) == -1)
            return -1;
        break;
    case KPR_NOT_FOUND:
        break;
    default:
        rbh_backend_error_printf("failed to retrieve 'enrichers' key in configuration: %s",
                                 strerror(errno));
        return -1;
    }

    return 0;
}
