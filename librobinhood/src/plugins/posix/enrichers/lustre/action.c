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

struct directive_option;

typedef int (*snprintf_value_t)(struct directive_option *option, char *output,
                                int max_length, const struct rbh_value *value);

struct directive_option {
    enum rbh_fsentry_property property;
    const char *name;
    snprintf_value_t primary_print_function;
    snprintf_value_t secondary_print_function;
};

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
snprintf_layout_pattern(__attribute__((unused)) struct directive_option *option,
                        char *output, int max_length,
                        const struct rbh_value *value)
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
snprintf_hsm_state(__attribute__((unused)) struct directive_option *option,
                   char *output, int max_length, const struct rbh_value *value)
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
snprintf_hash_flags(__attribute__((unused)) struct directive_option *option,
                    char *output, int max_length, const struct rbh_value *value)
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
snprintf_comp_flags(__attribute__((unused)) struct directive_option *option,
                    char *output, int max_length, const struct rbh_value *value)
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
snprintf_value(struct directive_option *option, char *output, int max_length,
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
          option->name, value->type);
    __builtin_unreachable();
}

static int
snprintf_value_array(__attribute__((unused)) struct directive_option *option,
                     char *output, int max_length,
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
        int tmp_length = option->secondary_print_function(
            option, array + index, size - index, &value->sequence.values[i]
        );

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
snprintf_hash_type(__attribute__((unused)) struct directive_option *option,
                   char *output, int max_length, const struct rbh_value *value)
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
snprintf_ost_count(__attribute__((unused)) struct directive_option *option,
                   char *output, int max_length, const struct rbh_value *value)
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
snprintf_flags_mirror_state(
    __attribute__((unused)) struct directive_option *option,
    char *output, int max_length, const struct rbh_value *value
)
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

static struct directive_option lustre_directive_table['z' - 'A' + 1] = {
    { 0 }, // 'A'
    { 0 }, // 'B'
    { 0 }, // 'C'
    { RBH_FP_INODE_XATTRS, "mdt_hash", snprintf_hash_type }, // 'D'
    { 0 }, // 'E'
    { RBH_FP_PARENT_ID, NULL }, // 'F'
    { 0 }, // 'G'
    { RBH_FP_INODE_XATTRS, "hsm_state", snprintf_hsm_state }, // 'H'
    { RBH_FP_INODE_XATTRS, "mdt_count", snprintf_value }, // 'I'
    { 0 }, // 'J'
    { 0 }, // 'K'
    { 0 }, // 'L'
    { RBH_FP_INODE_XATTRS, "mirror_count", snprintf_value }, // 'M'
    { RBH_FP_INODE_XATTRS, "magic", snprintf_value }, // 'N'
    { RBH_FP_INODE_XATTRS, "ost", snprintf_ost_count }, // 'O'
    { RBH_FP_INODE_XATTRS, "pool",
        snprintf_value_array, snprintf_value }, // 'P'
    { 0 }, // 'Q'
    { 0 }, // 'R'
    { RBH_FP_INODE_XATTRS, "stripe_count",
        snprintf_value_array, snprintf_value }, // 'S'
    { RBH_FP_INODE_XATTRS, "comp_count", snprintf_value }, // 'T'
    { 0 }, // 'U'
    { 0 }, // 'V'
    { 0 }, // 'W'
    { RBH_FP_INODE_XATTRS, "child_mdt_idx",
        snprintf_value_array, snprintf_value }, // 'X'
    { 0 }, // 'Y'
    { 0 }, // 'Z'
    { 0 }, // '['
    { 0 }, // '\'
    { 0 }, // ']'
    { 0 }, // '^'
    { 0 }, // '_'
    { 0 }, // '`'
    { 0 }, // 'a'
    { RBH_FP_INODE_XATTRS, "begin",
        snprintf_value_array, snprintf_value }, // 'b'
    { RBH_FP_INODE_XATTRS, "comp_flags",
        snprintf_value_array, snprintf_comp_flags }, // 'c'
    { RBH_FP_INODE_XATTRS, "mdt_hash_flags", snprintf_hash_flags }, // 'd'
    { RBH_FP_INODE_XATTRS, "end", snprintf_value_array, snprintf_value }, // 'e'
    { RBH_FP_ID, NULL }, // 'f'
    { RBH_FP_INODE_XATTRS, "flags", snprintf_flags_mirror_state }, // 'g'
    { RBH_FP_INODE_XATTRS, "hsm_archive_id", snprintf_value }, // 'h'
    { RBH_FP_INODE_XATTRS, "mdt_index", snprintf_value }, // 'i'
    { 0 }, // 'j'
    { 0 }, // 'k'
    { 0 }, // 'l'
    { RBH_FP_INODE_XATTRS, "mirror_state", snprintf_flags_mirror_state }, // 'm'
    { RBH_FP_INODE_XATTRS, "gen", snprintf_value }, // 'n'
    { RBH_FP_INODE_XATTRS, "ost", snprintf_value_array, snprintf_value }, // 'o'
    { RBH_FP_INODE_XATTRS, "project_id", snprintf_value }, // 'p'
    { 0 }, // 'q'
    { 0 }, // 'r'
    { RBH_FP_INODE_XATTRS, "stripe_size",
        snprintf_value_array, snprintf_value }, // 's'
    { RBH_FP_INODE_XATTRS, "pattern",
        snprintf_value_array, snprintf_layout_pattern }, // 't'
    { 0 }, // 'u'
    { 0 }, // 'v'
    { 0 }, // 'w'
    { RBH_FP_INODE_XATTRS, "extension_size",
        snprintf_value_array, snprintf_value }, // 'x'
    { 0 }, // 'y'
    { 0 }, // 'z'
};

enum known_directive
rbh_lustre_fill_entry_info(const struct rbh_fsentry *fsentry,
                           const char *format_string, size_t *index,
                           char *output, size_t *output_length, int max_length,
                           __attribute__((unused)) const char *backend)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;
    struct directive_option *option;
    const struct rbh_value *value;
    const struct lu_fid *fid;
    int tmp_length = 0;

    if (format_string[*index + 1] != RBH_NON_STANDARD_DIRECTIVE ||
        format_string[*index + 2] != RBH_LUSTRE_DIRECTIVE ||
        format_string[*index + 3] < 'A' || format_string[*index + 3] > 'z')
        return RBH_DIRECTIVE_UNKNOWN;

    option = &lustre_directive_table[format_string[*index + 3] - 'A'];
    switch (option->property) {
    case RBH_FP_ID:
        fid = rbh_lu_fid_from_id(&fsentry->id);
        tmp_length = snprintf(output, max_length, DFID, PFID(fid));
        break;
    case RBH_FP_PARENT_ID:
        if (fsentry->parent_id.size == 0) {
            tmp_length = snprintf(output, max_length, "None");
            break;
        }
        fid = rbh_lu_fid_from_id(&fsentry->parent_id);
        tmp_length = snprintf(output, max_length, DFID, PFID(fid));
        break;
    case RBH_FP_INODE_XATTRS:
        value = rbh_fsentry_find_inode_xattr(fsentry, option->name);
        tmp_length = option->primary_print_function(option, output, max_length,
                                                    value);
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
    struct directive_option *option;
    char str_filter_field[64];

    if (format_string[*index + 1] != RBH_NON_STANDARD_DIRECTIVE ||
        format_string[*index + 2] != RBH_LUSTRE_DIRECTIVE ||
        format_string[*index + 3] < 'A' || format_string[*index + 3] > 'z')
        return RBH_DIRECTIVE_UNKNOWN;

    option = &lustre_directive_table[format_string[*index + 3] - 'A'];
    if (!option->property)
        return RBH_DIRECTIVE_UNKNOWN;

    switch (option->property) {
    case RBH_FP_ID:
        rbh_projection_add(projection, str2filter_field("id"));
        break;
    case RBH_FP_PARENT_ID:
        rbh_projection_add(projection, str2filter_field("parent-id"));
        break;
    case RBH_FP_INODE_XATTRS:
        if (sprintf(str_filter_field, "xattrs.%s", option->name) < 0)
            return RBH_DIRECTIVE_ERROR;
        rbh_projection_add(projection, str2filter_field(str_filter_field));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        *index += 3;

    return rc;
}
