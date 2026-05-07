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

struct key_value {
    int64_t key;
    char *value;
};

static int
snprintf_key_values(char *output, int max_length,
                    struct key_value *key_values, int size, int64_t input)
{
    int tmp_length = 0;

    for (int i = 0; i < size && input; ++i) {
        if (!(input & key_values[i].key))
            continue;

        tmp_length += snprintf(output + tmp_length, max_length - tmp_length,
                               key_values[i].value);

        input ^= key_values[i].key;
        if (input)
            tmp_length += snprintf(output + tmp_length, max_length - tmp_length,
                                   "|");
    }

    if (input)
        return tmp_length + snprintf(output + tmp_length,
                                     max_length - tmp_length,
                                     "unknown");

    return tmp_length;
}

static int
snprintf_layout_pattern(__attribute__((unused)) const char *name, char *output,
                        int max_length, const struct rbh_value *value)
{
    /* Lustre has two ways to signify a file has a raid0 pattern, either
     * its pattern is 0 == LLAPI_LAYOUT_RAID0 == 0x00000000ULL or its
     * pattern is 1 == LOV_PATTERN_RAID0 == 0x001. So we have to check for both.
     */
    struct key_value key_values[] = {
        { LOV_PATTERN_RAID0, "raid0" },
        { LLAPI_LAYOUT_MDT, "mdt" },
        { LLAPI_LAYOUT_OVERSTRIPING, "overstriped" },
        { LOV_PATTERN_F_RELEASED, "released" }
    };

    if (value == NULL || value->type != RBH_VT_INT64)
        return snprintf(output, max_length, "None");

    if (value->int64 == LLAPI_LAYOUT_RAID0)
        return snprintf(output, max_length, "raid0");

    return snprintf_key_values(output, max_length, key_values,
                               ARRAY_SIZE(key_values), value->int64);
}

static int
snprintf_hsm_state(char *output, int max_length, const struct rbh_value *value)
{
    struct key_value key_values[] = {
        { HS_EXISTS, "exists" },
        { HS_DIRTY, "dirty" },
        { HS_RELEASED, "released" },
        { HS_ARCHIVED, "archived" },
        { HS_NORELEASE, "norelease" },
        { HS_NOARCHIVE, "noarchive" },
        { HS_LOST, "lost" },
    };

    if (value == NULL || value->type != RBH_VT_INT32 || value->int32 == 0)
        return snprintf(output, max_length, "None");

    return snprintf_key_values(output, max_length, key_values,
                               ARRAY_SIZE(key_values), value->int32);
}

static int
snprintf_hash_flags(char *output, int max_length, const struct rbh_value *value)
{
    struct key_value key_values[] = {
        { LMV_HASH_FLAG_FIXED, "fixed" },
        { LMV_HASH_FLAG_MERGE, "merge" },
        { LMV_HASH_FLAG_SPLIT, "split" },
        { LMV_HASH_FLAG_LOST_LMV, "lost_lmv" },
        { LMV_HASH_FLAG_BAD_TYPE, "bad_type" },
        { LMV_HASH_FLAG_MIGRATION, "migration" },
    };

    if (value == NULL || value->type != RBH_VT_INT32 || value->int32 == 0)
        return snprintf(output, max_length, "None");

    return snprintf_key_values(output, max_length, key_values,
                               ARRAY_SIZE(key_values), value->int32);
}

static int
snprintf_comp_flags(__attribute__((unused)) const char *name, char *output,
                    int max_length, const struct rbh_value *value)
{
    if (value == NULL || value->type != RBH_VT_INT32)
        return snprintf(output, max_length, "None");

    if (value->int32 == 0)
        return snprintf(output, max_length, "0");

    /* At the time of writing this, the 'comp_flags_table' and its associated
     * structure looks like this:
     *      static const struct comp_flag_name {
     *          enum lov_comp_md_entry_flags cfn_flag;
     *          const char *cfn_name;
     *      } comp_flags_table[];
     *
     * Which is the exact same size as our own struct key_value, so we do a
     * little trick and simply cast the table as our structure.
     */

    return snprintf_key_values(output, max_length,
                               (struct key_value *) comp_flags_table,
                               ARRAY_SIZE(comp_flags_table),
                               value->int32);
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

static int
snprintf_flags_mirror_state(char *output, int max_length,
                            const struct rbh_value *value)
{
    if (value == NULL || value->type != RBH_VT_INT32)
        return snprintf(output, max_length, "None");

    switch (value->int32) {
    case LCM_FL_NONE:
        return snprintf(output, max_length, "None");
    case LCM_FL_RDONLY:
        return snprintf(output, max_length, "ro");
    case LCM_FL_WRITE_PENDING:
        return snprintf(output, max_length, "wp");
    case LCM_FL_SYNC_PENDING:
        return snprintf(output, max_length, "sp");
    default:
        return snprintf(output, max_length, "unknown");
    }

    __builtin_unreachable();
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
    case 'b': // component begin
        value = rbh_fsentry_find_inode_xattr(fsentry, "begin");
        tmp_length = snprintf_value_array("begin", output, max_length, value,
                                          &snprintf_value);
        break;
    case 'c': // component flags
        value = rbh_fsentry_find_inode_xattr(fsentry, "comp_flags");
        tmp_length = snprintf_value_array("comp_flags", output, max_length,
                                          value, &snprintf_comp_flags);
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
    case 'd': // MDT hash flags
        value = rbh_fsentry_find_inode_xattr(fsentry, "mdt_hash_flags");
        tmp_length = snprintf_hash_flags(output, max_length, value);
        break;
    case 'D': // MDT hash type
        value = rbh_fsentry_find_inode_xattr(fsentry, "mdt_hash");
        tmp_length = snprintf_hash_type(output, max_length, value);
        break;
    case 'e': // component extension size
        value = rbh_fsentry_find_inode_xattr(fsentry, "extension_size");
        tmp_length = snprintf_value_array("extension_size", output, max_length,
                                          value, &snprintf_value);
        break;
    case 'E': // component end
        value = rbh_fsentry_find_inode_xattr(fsentry, "end");
        tmp_length = snprintf_value_array("end", output, max_length, value,
                                          &snprintf_value);
        break;
    case 'f': // FID
        fid = rbh_lu_fid_from_id(&fsentry->id);
        tmp_length = snprintf(output, max_length, DFID, PFID(fid));
        break;
    case 'F': // parent FID
        if (fsentry->parent_id.size == 0) {
            tmp_length = snprintf(output, max_length, "None");
            break;
        }
        fid = rbh_lu_fid_from_id(&fsentry->parent_id);
        tmp_length = snprintf(output, max_length, DFID, PFID(fid));
        break;
    case 'g': // flags
        value = rbh_fsentry_find_inode_xattr(fsentry, "flags");
        tmp_length = snprintf_flags_mirror_state(output, max_length, value);
        break;
    case 'h': // HSM archive id
        value = rbh_fsentry_find_inode_xattr(fsentry, "hsm_archive_id");
        tmp_length = snprintf_value("hsm_archive_id", output, max_length,
                                    value);
        break;
    case 'H': // HSM state
        value = rbh_fsentry_find_inode_xattr(fsentry, "hsm_state");
        tmp_length = snprintf_hsm_state(output, max_length, value);
        break;
    case 'i': // MDT index
        value = rbh_fsentry_find_inode_xattr(fsentry, "mdt_index");
        tmp_length = snprintf_value("mdt_index", output, max_length, value);
        break;
    case 'm': // mirror state
        value = rbh_fsentry_find_inode_xattr(fsentry, "mirror_state");
        tmp_length = snprintf_flags_mirror_state(output, max_length, value);
        break;
    case 'M': // mirror_count
        value = rbh_fsentry_find_inode_xattr(fsentry, "mirror_count");
        tmp_length = snprintf_value("mirror_count", output, max_length, value);
        break;
    case 'n': // generation
        value = rbh_fsentry_find_inode_xattr(fsentry, "gen");
        tmp_length = snprintf_value("gen", output, max_length, value);
        break;
    case 'N': // magic number
        value = rbh_fsentry_find_inode_xattr(fsentry, "magic");
        tmp_length = snprintf_value("magic", output, max_length, value);
        break;
    case 'o': // OSTs
        value = rbh_fsentry_find_inode_xattr(fsentry, "ost");
        tmp_length = snprintf_value_array("ost", output, max_length, value,
                                          &snprintf_value);
        break;
    case 'p': // project ID
        value = rbh_fsentry_find_inode_xattr(fsentry, "project_id");
        tmp_length = snprintf_value("project_id", output, max_length, value);
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
    case 'S': // stripe count
        value = rbh_fsentry_find_inode_xattr(fsentry, "stripe_count");
        tmp_length = snprintf_value_array("stripe_count", output, max_length,
                                          value, &snprintf_value);
        break;
    case 't': // component pattern
        value = rbh_fsentry_find_inode_xattr(fsentry, "pattern");
        tmp_length = snprintf_value_array("layout_pattern", output, max_length,
                                          value, snprintf_layout_pattern);
        break;
    case 'T': // component count
        value = rbh_fsentry_find_inode_xattr(fsentry, "comp_count");
        tmp_length = snprintf_value("comp_count", output, max_length, value);
        break;
    case 'X': // child MDT index
        value = rbh_fsentry_find_inode_xattr(fsentry, "child_mdt_idx");
        tmp_length = snprintf_value_array("child_mdt_idx", output, max_length,
                                          value, snprintf_value);
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
    case 'b': // component begin
        rbh_projection_add(projection, str2filter_field("xattrs.begin"));
        break;
    case 'c': // component flags
        rbh_projection_add(projection, str2filter_field("xattrs.comp_flags"));
        break;
    case 'C': // OST or MDT count
        rbh_projection_add(projection, str2filter_field("statx.mode"));
        rbh_projection_add(projection, str2filter_field("statx.type"));
        rbh_projection_add(projection, str2filter_field("xattrs.ost"));
        rbh_projection_add(projection, str2filter_field("xattrs.mdt_count"));
        break;
    case 'd': // MDT hash flags
        rbh_projection_add(projection,
                           str2filter_field("xattrs.mdt_hash_flags"));
        break;
    case 'D': // MDT hash type
        rbh_projection_add(projection, str2filter_field("xattrs.mdt_hash"));
        break;
    case 'e': // component extension size
        rbh_projection_add(projection,
                           str2filter_field("xattrs.extension_size"));
        break;
    case 'E': // component end
        rbh_projection_add(projection, str2filter_field("xattrs.end"));
        break;
    case 'f': // FID
        rbh_projection_add(projection, str2filter_field("id"));
        break;
    case 'F': // Parent FID
        rbh_projection_add(projection, str2filter_field("parent-id"));
        break;
    case 'g': // flags
        rbh_projection_add(projection, str2filter_field("xattrs.flags"));
        break;
    case 'h': // HSM archive id
        rbh_projection_add(projection,
                           str2filter_field("xattrs.hsm_archive_id"));
        break;
    case 'H': // HSM state
        rbh_projection_add(projection, str2filter_field("xattrs.hsm_state"));
        break;
    case 'i': // MDT index
        rbh_projection_add(projection, str2filter_field("xattrs.mdt_index"));
        break;
    case 'm': // mirror state
        rbh_projection_add(projection, str2filter_field("xattrs.mirror_state"));
        break;
    case 'M': // mirror count
        rbh_projection_add(projection, str2filter_field("xattrs.mirror_count"));
        break;
    case 'n': // generation
        rbh_projection_add(projection, str2filter_field("xattrs.gen"));
        break;
    case 'N': // magic number
        rbh_projection_add(projection, str2filter_field("xattrs.magic"));
        break;
    case 'o': // OSTs
        rbh_projection_add(projection, str2filter_field("xattrs.ost"));
        break;
    case 'p': // project ID
        rbh_projection_add(projection, str2filter_field("xattrs.project_id"));
        break;
    case 'P': // pool
        rbh_projection_add(projection, str2filter_field("xattrs.pool"));
        break;
    case 's': // stripe size
        rbh_projection_add(projection, str2filter_field("xattrs.stripe_size"));
        break;
    case 'S': // stripe count
        rbh_projection_add(projection, str2filter_field("xattrs.stripe_count"));
        break;
    case 't': // component pattern
        rbh_projection_add(projection, str2filter_field("xattrs.pattern"));
        break;
    case 'T': // component count
        rbh_projection_add(projection, str2filter_field("xattrs.comp_count"));
        break;
    case 'X': // child MDT index
        rbh_projection_add(projection,
                           str2filter_field("xattrs.child_mdt_idx"));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        *index += 3;

    return rc;
}
