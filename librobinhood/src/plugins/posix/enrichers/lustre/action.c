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
                           const char *format_string, size_t *index,
                           __attribute__((unused)) const char *backend)
{
    const struct lu_fid *fid;

    assert(format_string != NULL);
    assert(format_string[*index] == '%');
    assert(format_string[*index + 1] != '\0');

    (*index)++;

    switch (format_string[*index]) {
    case 'F':
        fid = rbh_lu_fid_from_id(&entry->id);
        return snprintf(output, max_length, DFID, PFID(fid));
    default:
        /* If we failed to identify the directive, let another plugin/extension
         * have a go at it
         */
        (*index)--;
    }

    return 0;
}

enum known_directive
rbh_lustre_fill_projection(struct rbh_filter_projection *projection,
                           const char *format_string, size_t *index)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;

    switch (format_string[*index + 1]) {
    case 'F': // FID
        rbh_projection_add(projection, str2filter_field("xattrs.fid"));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        (*index)++;

    return rc;
}
