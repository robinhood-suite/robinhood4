/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */


#include <stdbool.h>
#include <string.h>
#include <sysexits.h>


#include "robinhood/filters/core.h"
#include "robinhood/statx.h"
#include <robinhood.h>



// On fais la requete de db et on recupères les fsentry
// On va a partie des fsentry les parcourir pour faire les requetes aux fs
// Pour donner les actions peut etre passer du c au python tout le contexte ?
//  l faut y reflechir
struct rbh_mut_iterator *
rbh_collect_fsentry(const char *uri, struct rbh_filter *filter)
{
    struct rbh_backend *backend = rbh_backend_from_uri(uri, true);
    if (!backend)
        error(EXIT_FAILURE, errno, "rbh_backend_from_uri failed");

    struct rbh_filter_options options = {
        .skip = 0,
        .limit = 0,
        .skip_error = true,
        .one = false,
        .verbose = false,
        .dry_run = false
    };

    struct rbh_filter_output output = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = RBH_FP_ID | RBH_FP_NAME,
            .statx_mask = 0,
        }
    };

    struct rbh_mut_iterator *it = rbh_backend_filter(backend, filter, &options,
                                                     &output);
    if (!it)
        error(EXIT_FAILURE, errno, "rbh_backend_filter failed");

    return it;
}

void
print_fsentries(const char *uri, struct rbh_filter *filter)
{
    struct rbh_mut_iterator *it = rbh_collect_fsentry(uri, filter);

    while (true) {
        struct rbh_fsentry *fsentry;

        errno = 0;
        fsentry = rbh_mut_iter_next(it);

        if (fsentry == NULL) {
            if (errno == EAGAIN)
                continue;
            else if (errno == ENODATA)
                break;
            else if (errno == RBH_BACKEND_ERROR)
                error(EXIT_FAILURE, errno, "%s", rbh_backend_error);
            else
                error(EXIT_FAILURE, errno, "rbh_mut_iter_next failed");
        }

        printf("%s\n", fsentry->name ? fsentry->name : "(no name)");

        free(fsentry);
    }

    rbh_mut_iter_destroy(it);
}

