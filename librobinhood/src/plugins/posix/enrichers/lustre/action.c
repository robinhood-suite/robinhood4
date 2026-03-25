/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <inttypes.h>
#include <stdio.h>

#include <lustre/lustreapi.h>
#include <robinhood/action.h>
#include <robinhood/backend.h>
#include <robinhood/utils.h>
#include <robinhood.h>

int
rbh_lustre_fill_entry_info(char *output, int max_length,
                           const struct rbh_fsentry *entry,
                           const char *directive,
                           __attribute__((unused)) const char *backend)
{
    const struct lu_fid *fid;

    if (!directive || *directive == '\0')
        return 0;

    switch (*directive) {
    case 'F':
        if (!entry)
            return snprintf(output, max_length, "(NULL)");

        fid = rbh_lu_fid_from_id(&entry->id);
        if (!fid)
            return snprintf(output, max_length, "(NULL)");

        return snprintf(output, max_length, DFID, PFID(fid));
    }

    return 0;
}
