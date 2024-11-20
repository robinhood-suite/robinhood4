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
modifier2str(enum field_modifier modifier)
{
    switch (modifier) {
    case FM_SUM:
        return "$sum";
    default:
        return NULL;
    }
}

#define XATTR_ONSTACK_LENGTH 128

bool
get_modifier_field_strings(struct rbh_modifier_field *modifier_field,
                           char *modifier, char *field, char *key)
{
    char onstack[XATTR_ONSTACK_LENGTH];
    const char *tmp_modifier;
    char *buffer = onstack;
    const char *tmp_field;

    tmp_modifier = modifier2str(modifier_field->modifier);
    if (tmp_modifier == NULL)
        return false;

    strcpy(modifier, tmp_modifier);

    tmp_field = field2str(&modifier_field->field, &buffer, sizeof(onstack));
    if (tmp_field == NULL)
        return false;

    if (sprintf(field, "$%s", tmp_field) <= 0)
        return false;

    if (sprintf(key, "%s_%s", modifier, tmp_field) <= 0)
        return false;

    for (int j = 0; j < strlen(key); j++)
        if (key[j] == '.')
            key[j] = '_';

    return true;
}

static bool
insert_rbh_modifier_field(bson_t *bson, struct rbh_modifier_field *field)
{
    char modifier[256];
    char field_str[256];
    bson_t document;
    char key[512];

    if (!get_modifier_field_strings(field, modifier, field_str, key))
        return false;

    return BSON_APPEND_DOCUMENT_BEGIN(bson, &key[1], &document)
        && BSON_APPEND_UTF8(&document, modifier, field_str)
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

    if (!(bson_append_document_begin(bson, key, key_length, &document) &&
          BSON_APPEND_INT32(&document, "_id", 0)))
        return false;

    for (size_t i = 0; i < group->count; i++) {
        struct rbh_modifier_field *field = &group->fields[i];

        if (!insert_rbh_modifier_field(&document, field))
            return false;
    }

    return bson_append_document_end(bson, &document);
}
