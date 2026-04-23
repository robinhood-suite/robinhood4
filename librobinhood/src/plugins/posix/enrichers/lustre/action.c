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

// RobinHood3 uses 'R' for Lustre directives, do we keep the same letter ?
#define RBH_LUSTRE_DIRECTIVE_CATEGORY_CHARACTER 'R'

enum known_directive
rbh_lustre_fill_entry_info(const struct rbh_fsentry *fsentry,
                           const char *format_string, size_t *index,
                           char *output, size_t *output_length, int max_length,
                           __attribute__((unused)) const char *backend)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;
    const struct lu_fid *fid;
    int tmp_length = 0;

    if (format_string[*index + 1] != RBH_LUSTRE_DIRECTIVE_CATEGORY_CHARACTER)
        return RBH_DIRECTIVE_UNKNOWN;

    switch (format_string[*index + 2]) {
    case 'f':
        fid = rbh_lu_fid_from_id(&fsentry->id);
        tmp_length = snprintf(output, max_length, DFID, PFID(fid));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (tmp_length < 0)
        return RBH_DIRECTIVE_ERROR;

    if (rc == RBH_DIRECTIVE_KNOWN) {
        *output_length += tmp_length;
        *index += 2;
    }

    return rc;
}

enum known_directive
rbh_lustre_fill_projection(struct rbh_filter_projection *projection,
                           const char *format_string, size_t *index)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;

    if (format_string[*index + 1] != RBH_LUSTRE_DIRECTIVE_CATEGORY_CHARACTER)
        return RBH_DIRECTIVE_UNKNOWN;

    switch (format_string[*index + 2]) {
    case 'f': // FID
        rbh_projection_add(projection, str2filter_field("xattrs.fid"));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        *index += 2;

    return rc;
}
