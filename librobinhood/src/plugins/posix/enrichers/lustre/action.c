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

typedef int (*snprintf_value_t)(const char *name, char *output,
                                int max_length, const struct rbh_value *value);

static int
snprintf_layout_pattern(const char *name, char *output, int max_length,
                        const struct rbh_value *value)
{
    /* Lustre has two ways to signify a file has a raid0 pattern, either
     * its pattern is 0 == LLAPI_LAYOUT_RAID0 == 0x00000000ULL or its
     * pattern is 1 == LOV_PATTERN_RAID0 == 0x001. So we have to check for both.
     */
    int64_t ipatterns[] = {LOV_PATTERN_RAID0, LLAPI_LAYOUT_MDT,
                       LLAPI_LAYOUT_OVERSTRIPING, LOV_PATTERN_F_RELEASED};
    char *cpatterns[] = {"raid0", "mdt", "overstriped", "released"};
    int64_t entry_pattern;
    int tmp_length = 0;

    (void) name;

    if (value == NULL || value->type != RBH_VT_INT64)
        return snprintf(output, max_length, "None");

    entry_pattern = value->int64;
    if (entry_pattern == LLAPI_LAYOUT_RAID0)
        return snprintf(output, max_length, "raid0");

    for (int i = 0; i < sizeof(ipatterns) / sizeof(ipatterns[0]); ++i) {
        if (!(value->int64 & ipatterns[i]))
            continue;

        tmp_length += snprintf(output + tmp_length, max_length - tmp_length,
                               cpatterns[i]);
        entry_pattern ^= ipatterns[i];
        if (entry_pattern)
            tmp_length += snprintf(output + tmp_length, max_length - tmp_length,
                                   "|");
    }

    if (entry_pattern)
        return tmp_length + snprintf(output + tmp_length,
                                     max_length - tmp_length,
                                     "unknown");

    return tmp_length;
}

static int
snprintf_comp_flags(const char *name, char *output, int max_length,
                    const struct rbh_value *value)
{
    int64_t entry_pattern;
    int tmp_length = 0;
    (void) name;

    if (value == NULL || value->type != RBH_VT_INT32)
        return snprintf(output, max_length, "None");

    entry_pattern = value->int32;
    if (entry_pattern == 0)
        return snprintf(output, max_length, "0");

    for (int i = 0; i < ARRAY_SIZE(comp_flags_table); i++) {
        enum lov_comp_md_entry_flags comp_flag = comp_flags_table[i].cfn_flag;
        const char *comp_name = comp_flags_table[i].cfn_name;

        if (!(value->int32 & comp_flag))
            continue;

        tmp_length += snprintf(output + tmp_length, max_length - tmp_length,
                               comp_name);
        entry_pattern ^= comp_flag;
        if (entry_pattern)
            tmp_length += snprintf(output + tmp_length, max_length - tmp_length,
                                   "|");
    }

    if (entry_pattern)
        return tmp_length + snprintf(output + tmp_length,
                                     max_length - tmp_length,
                                     "unknown");

    return tmp_length;
}

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
    case RBH_VT_STRING:
        return snprintf(output, max_length, "%s", value->string);
    case RBH_VT_SEQUENCE:
        /* If we try to print a sequence without going through
         * 'snprintf_value_array' first, we instead print the size of the
         * sequence.
         */
        return snprintf(output, max_length, "%lu", value->sequence.count);
    default:
        break;
    }

    error(EXIT_FAILURE, EINVAL, "unexpected type in fsentry for value '%s': %d",
          name, value->type);
    __builtin_unreachable();
}

static int
snprintf_value_array(const char *name, char *output, int max_length,
                     const struct rbh_value *value,
                     snprintf_value_t print_value)
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
        int tmp_length = print_value(name, array + index, size - index,
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

static int
snprintf_hash_type(char *output, int max_length, const struct rbh_value *value)
{
    if (value == NULL || value->type != RBH_VT_INT32)
        return snprintf(output, max_length, "None");

    switch (value->int32) {
    case LMV_HASH_TYPE_ALL_CHARS:
        return snprintf(output, max_length, "all_char");
    case LMV_HASH_TYPE_FNV_1A_64:
        return snprintf(output, max_length, "fnv_1a_64");
    case LMV_HASH_TYPE_CRUSH:
        return snprintf(output, max_length, "crush");
    case LMV_HASH_TYPE_CRUSH2:
        return snprintf(output, max_length, "crush2");
    default:
        return snprintf(output, max_length, "unknown");
    }

    __builtin_unreachable();
}

static int
snprintf_ost_count(char *output, int max_length, const struct rbh_value *value)
{
    long i;

    if (value == NULL ||
        value->type != RBH_VT_SEQUENCE ||
        value->sequence.count == 0 ||
        value->sequence.values[0].type != RBH_VT_INT64)
        return snprintf(output, max_length, "None");

    for (i = value->sequence.count - 1;
         i >= 0 && value->sequence.values[i].int64 == -1;
         --i);

    return snprintf(output, max_length, "%ld", i + 1);
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
    case 'c': // stripe count
        value = rbh_fsentry_find_inode_xattr(fsentry, "stripe_count");
        tmp_length = snprintf_value_array("stripe_count", output, max_length,
                                          value, &snprintf_value);
        break;
    case 'C': // OST or MDT count
        if (S_ISDIR(fsentry->statx->stx_mode)) {
            value = rbh_fsentry_find_inode_xattr(fsentry, "mdt_count");
            tmp_length = snprintf_value("mdt_count", output, max_length, value);
        } else {
            value = rbh_fsentry_find_inode_xattr(fsentry, "ost");
            tmp_length = snprintf_ost_count(output, max_length, value);
        }
        break;
    case 'e': // extension size
        value = rbh_fsentry_find_inode_xattr(fsentry, "extension_size");
        tmp_length = snprintf_value_array("extension_size", output, max_length,
                                          value, &snprintf_value);
        break;
    case 'f': // FID
        fid = rbh_lu_fid_from_id(&fsentry->id);
        tmp_length = snprintf(output, max_length, DFID, PFID(fid));
        break;
    case 'g': // generation
        value = rbh_fsentry_find_inode_xattr(fsentry, "gen");
        tmp_length = snprintf_value("gen", output, max_length, value);
        break;
    case 'h': // hash type
        value = rbh_fsentry_find_inode_xattr(fsentry, "mdt_hash");
        tmp_length = snprintf_hash_type(output, max_length, value);
        break;
    case 'i': // MDT index
        value = rbh_fsentry_find_inode_xattr(fsentry, "mdt_index");
        tmp_length = snprintf_value("mdt_index", output, max_length, value);
        break;
    case 'I': // project ID
        value = rbh_fsentry_find_inode_xattr(fsentry, "project_id");
        tmp_length = snprintf_value("project_id", output, max_length, value);
        break;
    case 'l': // component flags
        value = rbh_fsentry_find_inode_xattr(fsentry, "comp_flags");
        tmp_length = snprintf_value_array("comp_flags", output, max_length,
                                          value, &snprintf_comp_flags);
        break;
    case 'o': // OSTs
        value = rbh_fsentry_find_inode_xattr(fsentry, "ost");
        tmp_length = snprintf_value_array("ost", output, max_length, value,
                                          &snprintf_value);
        break;
    case 'p': // parent FID
        if (fsentry->parent_id.size == 0) {
            tmp_length = snprintf(output, max_length, "None");
            break;
        }
        fid = rbh_lu_fid_from_id(&fsentry->parent_id);
        tmp_length = snprintf(output, max_length, DFID, PFID(fid));
        break;
    case 'P': // pool
        value = rbh_fsentry_find_inode_xattr(fsentry, "pool");
        tmp_length = snprintf_value_array("pool", output, max_length, value,
                                          &snprintf_value);
        break;
    case 's': // stripe size
        value = rbh_fsentry_find_inode_xattr(fsentry, "stripe_size");
        tmp_length = snprintf_value_array("stripe_size", output, max_length,
                                          value, &snprintf_value);
        break;
    case 't': // layout pattern
        value = rbh_fsentry_find_inode_xattr(fsentry, "pattern");
        tmp_length = snprintf_value_array("layout_pattern", output, max_length,
                                          value, snprintf_layout_pattern);
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
    case 'c': // stripe count
        rbh_projection_add(projection, str2filter_field("xattrs.stripe_count"));
        break;
    case 'C': // OST or MDT count
        rbh_projection_add(projection, str2filter_field("statx.mode"));
        rbh_projection_add(projection, str2filter_field("statx.type"));
        rbh_projection_add(projection, str2filter_field("xattrs.ost"));
        rbh_projection_add(projection, str2filter_field("xattrs.mdt_count"));
        break;
    case 'e': // FID
        rbh_projection_add(projection,
                           str2filter_field("xattrs.extension_size"));
        break;
    case 'f': // FID
        rbh_projection_add(projection, str2filter_field("id"));
        break;
    case 'g': // generation
        rbh_projection_add(projection, str2filter_field("xattrs.gen"));
        break;
    case 'h': // hash type
        rbh_projection_add(projection, str2filter_field("xattrs.mdt_hash"));
        break;
    case 'i': // mdt index
        rbh_projection_add(projection, str2filter_field("xattrs.mdt_index"));
        break;
    case 'I': // project ID
        rbh_projection_add(projection, str2filter_field("xattrs.project_id"));
        break;
    case 'l': // component flags
        rbh_projection_add(projection, str2filter_field("xattrs.comp_flags"));
        break;
    case 'o': // OSTs
        rbh_projection_add(projection, str2filter_field("xattrs.ost"));
        break;
    case 'p': // Parent FID
        rbh_projection_add(projection, str2filter_field("parent-id"));
        break;
    case 'P': // pool
        rbh_projection_add(projection, str2filter_field("xattrs.pool"));
        break;
    case 's': // stripe size
        rbh_projection_add(projection, str2filter_field("xattrs.stripe_size"));
        break;
    case 't': // layout pattern
        rbh_projection_add(projection, str2filter_field("xattrs.pattern"));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        *index += 3;

    return rc;
}
