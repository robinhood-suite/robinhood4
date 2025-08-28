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
#include "robinhood/utils.h"
#include "robinhood/backends/s3_extension.h"

int
rbh_s3_backend_load_iterator(const struct rbh_backend_plugin *self,
                             void *backend, const char *iterator,
                             const char *type)
{
    struct s3_backend *s3 = (struct s3_backend *) backend;
    const struct rbh_s3_extension *extension;

    extension = rbh_s3_load_extension(&self->plugin, iterator);
    if (!extension) {
        rbh_backend_error_printf("failed to load iterator '%s' for backend '%s'",
                                 iterator, type);
        return -1;
    }

    s3->iter_new = extension->iter_new;

    return 0;
}
