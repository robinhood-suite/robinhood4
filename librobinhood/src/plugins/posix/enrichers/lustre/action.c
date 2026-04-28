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

#define RBH_LUSTRE_DIRECTIVE 'L'

static int
snprintf_value(const char *name, char *output, int max_length,
               const struct rbh_value *value)
{
    if (value == NULL)
        return snprintf(output, max_length, "None");

    switch (value->type) {
    case RBH_VT_INT32:
        return snprintf(output, max_length, "%d", value->int32);
    case RBH_VT_INT64:
        return snprintf(output, max_length, "%ld", value->int64);
    default:
        break;
    }

    error(EXIT_FAILURE, EINVAL, "unexpected type in fsentry for value '%s': %d",
          name, value->type);
    __builtin_unreachable();
}

static int
snprintf_value_array(const char *name, char *output, int max_length,
                     const struct rbh_value *value)
{
    char array[128];
    size_t size;
    int index;

    if (value == NULL || value->type != RBH_VT_SEQUENCE)
        return snprintf(output, max_length, "None");

    size = sizeof(array);
    array[0] = '[';
    index = 1;

    for (int i = 0; i < value->sequence.count; ++i) {
        int tmp_length = snprintf_value(name, array + index, size - index,
                                        &value->sequence.values[i]);

        if (tmp_length < 0)
            return -1;

        index += tmp_length;
        if (index >= size - 2) {
            index = size - 2;
            break;
        }

        if (i < value->sequence.count - 1) {
            array[index++] = ',';
            array[index++] = ' ';
        }
    }

    array[index] = ']';
    array[index + 1] = '\0';
    return snprintf(output, max_length, "%s", array);
}

enum known_directive
rbh_lustre_fill_entry_info(const struct rbh_fsentry *fsentry,
                           const char *format_string, size_t *index,
                           char *output, size_t *output_length, int max_length,
                           __attribute__((unused)) const char *backend)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;
    const struct rbh_value *value;
    const struct lu_fid *fid;
    int tmp_length = 0;

    if (format_string[*index + 1] != RBH_NON_STANDARD_DIRECTIVE ||
        format_string[*index + 2] != RBH_LUSTRE_DIRECTIVE)
        return RBH_DIRECTIVE_UNKNOWN;

    switch (format_string[*index + 3]) {
    case 'f': // FID
        fid = rbh_lu_fid_from_id(&fsentry->id);
        tmp_length = snprintf(output, max_length, DFID, PFID(fid));
        break;
    case 'g': // generation
        value = rbh_fsentry_find_inode_xattr(fsentry, "gen");
        tmp_length = snprintf_value("gen", output, max_length, value);
        break;
    case 'o': // OSTs
        value = rbh_fsentry_find_inode_xattr(fsentry, "ost");
        tmp_length = snprintf_value_array("ost", output, max_length, value);
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (tmp_length < 0)
        return RBH_DIRECTIVE_ERROR;

    if (rc == RBH_DIRECTIVE_KNOWN) {
        *output_length += tmp_length;
        *index += 3;
    }

    return rc;
}

enum known_directive
rbh_lustre_fill_projection(struct rbh_filter_projection *projection,
                           const char *format_string, size_t *index)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;

    if (format_string[*index + 1] != RBH_NON_STANDARD_DIRECTIVE ||
        format_string[*index + 2] != RBH_LUSTRE_DIRECTIVE)
        return RBH_DIRECTIVE_UNKNOWN;

    switch (format_string[*index + 3]) {
    case 'f': // FID
        rbh_projection_add(projection, str2filter_field("xattrs.fid"));
        break;
    case 'g': // generation
        rbh_projection_add(projection, str2filter_field("xattrs.gen"));
        break;
    case 'o': // OSTs
        rbh_projection_add(projection, str2filter_field("xattrs.ost"));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        *index += 3;

    return rc;
}
