/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/statx.h"

#include "mongo.h"

const char *
accumulator2str(enum field_accumulator accumulator)
{
    switch (accumulator) {
    case FA_AVG:
        return "$avg";
    case FA_MAX:
        return "$max";
    case FA_MIN:
        return "$min";
    case FA_SUM:
        return "$sum";
    default:
        return NULL;
    }
}

#define XATTR_ONSTACK_LENGTH 128

bool
get_accumulator_field_strings(struct rbh_accumulator_field *accumulator_field,
                              char *accumulator, char *field, char *key)
{
    char onstack[XATTR_ONSTACK_LENGTH];
    const char *tmp_accumulator;
    char *buffer = onstack;
    const char *tmp_field;

    tmp_accumulator = accumulator2str(accumulator_field->accumulator);
    if (tmp_accumulator == NULL)
        return false;

    strcpy(accumulator, tmp_accumulator);

    tmp_field = field2str(&accumulator_field->field, &buffer, sizeof(onstack));
    if (tmp_field == NULL)
        return false;

    if (sprintf(field, "$%s", tmp_field) <= 0)
        return false;

    if (sprintf(key, "%s_%s", accumulator, tmp_field) <= 0)
        return false;

    escape_field_path(key);

    return true;
}

bool
is_set_for_range_needed(const struct rbh_group_fields *group)
{
    for (size_t i = 0; i < group->id_count; i++)
        if (group->id_fields[i].boundaries_count != 0)
            return true;

    return false;
}

static bool
get_field_range_key(char *range_key, const struct rbh_range_field *field)
{
    char onstack[XATTR_ONSTACK_LENGTH];
    char *buffer = onstack;
    const char *tmp_field;

    tmp_field = field2str(&field->field, &buffer, sizeof(onstack));
    if (tmp_field == NULL)
        return false;

    if (sprintf(range_key, "%s_range", tmp_field) <= 0)
        return false;

    escape_field_path(range_key);

    return true;
}

static bool
bson_append_case(bson_t *bson, int stage_number, const char *field,
                 int64_t lower, int64_t upper)
{
    bson_t *case_document;
    const char *key;
    char str[16];

    bson_uint32_to_string(stage_number, &key, str, sizeof(str));

    case_document = BCON_NEW(
        "case",
            "{", "$lte",
                "[", BCON_UTF8(field), BCON_INT64(upper), "]",
            "}",
        "then",
            "[", BCON_INT64(lower), BCON_INT64(upper), "]");

    return BSON_APPEND_DOCUMENT(bson, key, case_document);
}

static bool
bson_append_switch(bson_t *bson, const struct rbh_range_field *field)
{
    char onstack[XATTR_ONSTACK_LENGTH];
    bson_t branches_document;
    bson_t switch_document;
    char *buffer = onstack;
    const char *tmp_field;
    char field_str[256];

    if (!BSON_APPEND_DOCUMENT_BEGIN(bson, "$switch", &switch_document))
        return false;

    if (!BSON_APPEND_ARRAY_BEGIN(&switch_document, "branches",
                                 &branches_document))
        return false;

    tmp_field = field2str(&field->field, &buffer, sizeof(onstack));
    if (tmp_field == NULL)
        return false;

    if (sprintf(field_str, "$%s", tmp_field) <= 0)
        return false;

    for (int i = 0; i < field->boundaries_count - 1; i++)
        if (!bson_append_case(&branches_document, i, field_str,
                              field->boundaries[i], field->boundaries[i + 1]))
            return false;

    return bson_append_array_end(&switch_document, &branches_document)
        && BSON_APPEND_UTF8(&switch_document, "default", "other")
        && bson_append_document_end(bson, &switch_document);
}

static bool
bson_append_range(bson_t *bson, const struct rbh_range_field *field)
{
    bson_t range_document;
    char range_key[256];

    if (!get_field_range_key(range_key, field))
        return false;

    return BSON_APPEND_DOCUMENT_BEGIN(bson, range_key, &range_document)
        && bson_append_switch(&range_document, field)
        && bson_append_document_end(bson, &range_document);
}

bool
bson_append_aggregate_set_stage(bson_t *bson, const char *key,
                                size_t key_length,
                                const struct rbh_group_fields *group)
{

    bson_t set_document;

    if (!bson_append_document_begin(bson, key, key_length, &set_document))
        return false;

    for (int i = 0; i < group->id_count; i++)
        if (group->id_fields[i].boundaries_count != 0)
            if (!bson_append_range(&set_document, &group->id_fields[i]))
                return false;

    return bson_append_document_end(bson, &set_document);
}

static bool
insert_group_id_fields(bson_t *bson, const struct rbh_group_fields *group)
{
    bson_t subdoc;

    if (group->id_count == 0)
        return BSON_APPEND_INT32(bson, "_id", 0);

    if (!BSON_APPEND_DOCUMENT_BEGIN(bson, "_id", &subdoc))
        return false;

    for (size_t i = 0; i < group->id_count; i++) {
        struct rbh_range_field *field = &group->id_fields[i];
        char onstack[XATTR_ONSTACK_LENGTH];
        char *buffer = onstack;
        const char *tmp_field;
        char field_str[256];
        char field_key[256];

        tmp_field = field2str(&field->field, &buffer, sizeof(onstack));
        if (tmp_field == NULL)
            return false;

        if (sprintf(field_str, "$%s", tmp_field) <= 0 ||
            sprintf(field_key, "%s", tmp_field) <= 0)
            return false;

        escape_field_path(field_key);

        if (!BSON_APPEND_UTF8(&subdoc, field_key, field_str))
            return false;
    }

    return bson_append_document_end(bson, &subdoc);
}

static bool
insert_rbh_accumulator_field(bson_t *bson, struct rbh_accumulator_field *field)
{
    char accumulator[256];
    char field_str[256];
    bson_t document;
    char key[512];

    if (!get_accumulator_field_strings(field, accumulator, field_str, key))
        return false;

    return BSON_APPEND_DOCUMENT_BEGIN(bson, &key[1], &document)
        && BSON_APPEND_UTF8(&document, accumulator, field_str)
        && bson_append_document_end(bson, &document);
}

/* The resulting bson will be as such:
 * { $group: {_id: '', <key>: {<accumulator>: <field>}}}
 */
bool
bson_append_aggregate_group_stage(bson_t *bson, const char *key,
                                  size_t key_length,
                                  const struct rbh_group_fields *group)
{
    bson_t document;

    if (!bson_append_document_begin(bson, key, key_length, &document))
        return false;

    if (!insert_group_id_fields(&document, group))
        return false;

    for (size_t i = 0; i < group->acc_count; i++) {
        struct rbh_accumulator_field *field = &group->acc_fields[i];

        if (!insert_rbh_accumulator_field(&document, field))
            return false;
    }

    return bson_append_document_end(bson, &document);
}
