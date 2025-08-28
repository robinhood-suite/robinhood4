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
#include "robinhood/utils.h"
#include "robinhood/value.h"

#include "robinhood/backends/posix_extension.h"

static int
rbh_posix_backend_load_iterator(const struct rbh_backend_plugin *self,
                                void *backend, const char *iterator,
                                const char *type)
{
    struct posix_backend *posix = (struct posix_backend *) backend;
    const struct rbh_posix_extension *extension;

    if (!strcmp(iterator, "fts"))
        return 0;

    extension = rbh_posix_load_extension(&self->plugin, iterator);
    if (!extension) {
        rbh_backend_error_printf("failed to load iterator '%s' for backend '%s'",
                                 iterator, type);
        return -1;
    }

    posix->iter_new = extension->iter_new;

    return 0;
}

static int
rbh_posix_backend_load_enrichers(const struct rbh_backend_plugin *self,
                                 void *backend,
                                 const struct rbh_value *enrichers,
                                 const char *type)
{
    struct posix_backend *posix = (struct posix_backend *) backend;
    size_t count = enrichers->sequence.count + 1; /* NULL terminated */
    const struct rbh_posix_extension *extension;

    /* if we arrive here, we have at least one enricher to load */
    assert(count >= 2);

    posix->enrichers = xmalloc(sizeof(*posix->enrichers) * count);

    for (size_t i = 0; i < enrichers->sequence.count; i++) {
        const struct rbh_value *value = &enrichers->sequence.values[i];

        assert(value->type == RBH_VT_STRING);

        extension = rbh_posix_load_extension(&self->plugin, value->string);
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
rbh_posix_backend_load_extensions(const struct rbh_backend_plugin *self,
                                  void *backend, const char *type)
{
    enum key_parse_result rc;
    struct rbh_value value;

    rc = rbh_config_find_backend(type, "iterator", &value, RBH_VT_STRING);
    switch (rc) {
    case KPR_FOUND:
        if (rbh_posix_backend_load_iterator(self, backend, value.string, type))
            return -1;
        break;
    case KPR_NOT_FOUND:
        break;
    default:
        rbh_backend_error_printf("failed to retrieve 'backends/%s/iterator': %s",
                                 type, strerror(errno));
        return -1;
    }

    rc = rbh_config_find_backend(type, "enrichers", &value, RBH_VT_SEQUENCE);
    switch (rc) {
    case KPR_FOUND:
        if (rbh_posix_backend_load_enrichers(self, backend, &value, type))
            return -1;
        break;
    case KPR_NOT_FOUND:
        break;
    default:
        rbh_backend_error_printf("failed to retrieve 'backends/%s/enrichers' : %s",
                                 type, strerror(errno));
        return -1;
    }

    return 0;
}
