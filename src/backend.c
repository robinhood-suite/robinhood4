/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/backend.h"

int
rbh_generic_backend_get_option(struct rbh_backend *backend, unsigned int option,
                               void *data, size_t *data_size)
{
    switch (option) {
    case RBH_GBO_DEPRECATED:
        errno = ENOTSUP;
        return -1;
    }

    errno = EINVAL;
    return -1;
}

int
rbh_generic_backend_set_option(struct rbh_backend *backend, unsigned int option,
                               const void *data, size_t data_size)
{
    switch (option) {
    case RBH_GBO_DEPRECATED:
        errno = ENOTSUP;
        return -1;
    }

    errno = EINVAL;
    return -1;
}
