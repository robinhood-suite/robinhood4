/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "internals.h"

struct rbh_mut_iterator *
sqlite_backend_report(void *backend, const struct rbh_filter *filter,
                      const struct rbh_group_fields *group,
                      const struct rbh_filter_options *options,
                      const struct rbh_filter_output *output)
{
    struct sqlite_query_options query_options = {0};
    struct sqlite_filter_where where = {0};

    if (rbh_filter_validate(filter))
        return NULL;

    if (!filter2where_clause(filter, &where))
        return false;

    if (!options2sql(options, &query_options))
        return false;

    errno = ENODATA;
    return NULL;
}
