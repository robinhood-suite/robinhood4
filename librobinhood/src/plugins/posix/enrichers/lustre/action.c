/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <assert.h>
#include <stdio.h>

#include <lustre/lustreapi.h>

#include <robinhood/backend.h>
#include <robinhood/fsentry.h>
#include <robinhood/projection.h>
#include <robinhood/utils.h>

#include "lustre_internals.h"

int
rbh_lustre_fill_entry_info(char *output, int max_length,
                           const struct rbh_fsentry *entry,
                           const char *directive,
                           __attribute__((unused)) const char *backend)
{
    const struct lu_fid *fid;

    assert(directive != NULL);
    assert(*directive != '\0');

    switch (*directive) {
    case 'F':
        fid = rbh_lu_fid_from_id(&entry->id);
        return snprintf(output, max_length, DFID, PFID(fid));
    }

    return 0;
}

int
rbh_lustre_fill_projection(struct rbh_filter_projection *projection,
                           const char *directive)
{
    assert(directive != NULL);
    assert(*directive != '\0');

    switch (*directive) {
    case 'F': // FID
        rbh_projection_add(projection, str2filter_field("xattrs.fid"));
        break;
    default:
        return 0;
    }

    return 1;
}
