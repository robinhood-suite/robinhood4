/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/filter.h"

#include "mongo.h"

/*----------------------------------------------------------------------------*
 |                            bson_append_filter()                            |
 *----------------------------------------------------------------------------*/

/* The following helpers should only be used on a valid filter */

static const char * const FOP2STR[] = {
    [RBH_FOP_EQUAL]             = "$eq",
    [RBH_FOP_LOWER_THAN]        = "$lt",
    [RBH_FOP_LOWER_OR_EQUAL]    = "$le",
    [RBH_FOP_GREATER_THAN]      = "$gt",
    [RBH_FOP_GREATER_OR_EQUAL]  = "$ge",
    [RBH_FOP_IN]                = "$in",
    [RBH_FOP_REGEX]             = "$regex",
    [RBH_FOP_BITS_ANY_SET]      = "$bitsAnySet",
    [RBH_FOP_BITS_ALL_SET]      = "$bitsAllSet",
    [RBH_FOP_BITS_ANY_CLEAR]    = "$bitsAnyClear",
    [RBH_FOP_BITS_ALL_CLEAR]    = "$bitsAllClear",
    [RBH_FOP_AND]               = "$and",
    [RBH_FOP_OR]                = "$or",
};

static const char * const NEGATED_FOP2STR[] = {
    [RBH_FOP_EQUAL]             = "$ne",
    [RBH_FOP_LOWER_THAN]        = "$ge",
    [RBH_FOP_LOWER_OR_EQUAL]    = "$gt",
    [RBH_FOP_GREATER_THAN]      = "$le",
    [RBH_FOP_GREATER_OR_EQUAL]  = "$lt",
    [RBH_FOP_IN]                = "$nin",
    [RBH_FOP_REGEX]             = "$not", /* This is not a mistake */
    [RBH_FOP_BITS_ANY_SET]      = "$bitsAllClear",
    [RBH_FOP_BITS_ALL_SET]      = "$bitsAnyClear",
    [RBH_FOP_BITS_ANY_CLEAR]    = "$bitsAllSet",
    [RBH_FOP_BITS_ALL_CLEAR]    = "$bitsAnySet",
    [RBH_FOP_AND]               = "$or",
    [RBH_FOP_OR]                = "$and",
};

static const char *
fop2str(enum rbh_filter_operator op, bool negate)
{
    return negate ? NEGATED_FOP2STR[op] : FOP2STR[op];
}

static const char * const FIELD2STR[] = {
    [RBH_FF_ID]         = MFP_ID,
    [RBH_FF_PARENT_ID]  = MFP_NAMESPACE "." MFP_PARENT_ID,
    [RBH_FF_NAME]       = MFP_NAMESPACE "." MFP_NAME,
    [RBH_FF_TYPE]       = MFP_STATX "." MFP_STATX_TYPE,
    [RBH_FF_ATIME]      = MFP_STATX "." MFP_STATX_ATIME "." MFP_STATX_TIMESTAMP_SEC,
    [RBH_FF_CTIME]      = MFP_STATX "." MFP_STATX_CTIME "." MFP_STATX_TIMESTAMP_SEC,
    [RBH_FF_MTIME]      = MFP_STATX "." MFP_STATX_MTIME "." MFP_STATX_TIMESTAMP_SEC,
};

static bool
bson_append_filter_value(bson_t *bson, const char *key, size_t key_length,
                         const struct rbh_filter_value *value);

static bool
bson_append_list(bson_t *bson, const char *key, size_t key_length,
                 const struct rbh_filter_value *values, size_t count)
{
    bson_t array;

    if (!bson_append_array_begin(bson, key, key_length, &array))
        return false;

    for (uint32_t i = 0; i < count; i++) {
        char str[16];

        key_length = bson_uint32_to_string(i, &key, str, sizeof(str));
        if (!bson_append_filter_value(&array, key, key_length, &values[i]))
            return false;
    }

    return bson_append_array_end(bson, &array);
}

static bool
_bson_append_regex(bson_t *bson, const char *key, size_t key_length,
                    const char *regex, unsigned int options)
{
    char mongo_regex_options[8] = {'s', '\0',};
    uint8_t i = 1;

    if (options & RBH_FRO_CASE_INSENSITIVE)
        mongo_regex_options[i++] = 'i';

    return bson_append_regex(bson, key, key_length, regex, mongo_regex_options);
}

static bool
bson_append_filter_value(bson_t *bson, const char *key, size_t key_length,
                         const struct rbh_filter_value *value)
{
    switch (value->type) {
    case RBH_FVT_BINARY:
        return _bson_append_binary(bson, key, key_length, BSON_SUBTYPE_BINARY,
                                   value->binary.data, value->binary.size)
            && (value->binary.size != 0
                    || BSON_APPEND_INT32(bson, "$type", BSON_TYPE_NULL));
    case RBH_FVT_INT32:
        return bson_append_int32(bson, key, key_length, value->int32);
    case RBH_FVT_INT64:
        return bson_append_int64(bson, key, key_length, value->int64);
    case RBH_FVT_STRING:
        return bson_append_utf8(bson, key, key_length, value->string,
                                strlen(value->string));
    case RBH_FVT_REGEX:
        return _bson_append_regex(bson, key, key_length, value->regex.string,
                                   value->regex.options);
    case RBH_FVT_TIME:
        /* Since we do not use the native MongoDB date type, we must adapt our
         * comparison operators.
         */
        return bson_append_int64(bson, key, key_length, value->time);
    case RBH_FVT_LIST:
        return bson_append_list(bson, key, key_length, value->list.elements,
                                value->list.count);
    }
    __builtin_unreachable();
}

#define BSON_APPEND_FILTER_VALUE(bson, key, filter_value) \
    bson_append_filter_value(bson, key, strlen(key), filter_value)

static bool
bson_append_comparison_filter(bson_t *bson, const struct rbh_filter *filter,
                              bool negate)
{
    bson_t document;

    if (filter->op == RBH_FOP_REGEX && !negate)
        /* The regex operator is tricky: $not and $regex are not compatible.
         *
         * The workaround is not to use the $regex operator and replace it with
         * the "/pattern/" syntax.
         */
        return BSON_APPEND_FILTER_VALUE(bson, FIELD2STR[filter->compare.field],
                                        &filter->compare.value);

    return BSON_APPEND_DOCUMENT_BEGIN(bson, FIELD2STR[filter->compare.field],
                                      &document)
        && BSON_APPEND_FILTER_VALUE(&document, fop2str(filter->op, negate),
                                    &filter->compare.value)
        && bson_append_document_end(bson, &document);
}

static bool
_bson_append_filter(bson_t *bson, const struct rbh_filter *filter, bool negate);

static bool
bson_append_logical_filter(bson_t *bson, const struct rbh_filter *filter,
                           bool negate)
{
    bson_t array;

    if (filter->op == RBH_FOP_NOT)
        return _bson_append_filter(bson, filter->logical.filters[0], !negate);

    if (!BSON_APPEND_ARRAY_BEGIN(bson, fop2str(filter->op, negate), &array))
        return false;

    for (uint32_t i = 0; i < filter->logical.count; i++) {
        const char *key;
        size_t length;
        char str[16];

        length = bson_uint32_to_string(i, &key, str, sizeof(str));
        if (!bson_append_filter(&array, key, length, filter->logical.filters[i],
                                negate))
            return false;
    }

    return bson_append_array_end(bson, &array);
}

static bool
bson_append_null_filter(bson_t *bson, bool negate)
{
    bson_t document;

    /* XXX: I would prefer to use {$expr: !negate}, but it is not
     *      supported on servers until version 3.6.
     */
    return BSON_APPEND_DOCUMENT_BEGIN(bson, "_id", &document)
        && BSON_APPEND_BOOL(&document, "$exists", !negate)
        && bson_append_document_end(bson, &document);
}

static bool
_bson_append_filter(bson_t *bson, const struct rbh_filter *filter, bool negate)
{
    if (filter == NULL)
        return bson_append_null_filter(bson, negate);

    if (rbh_is_comparison_operator(filter->op))
        return bson_append_comparison_filter(bson, filter, negate);
    return bson_append_logical_filter(bson, filter, negate);
}

bool
bson_append_filter(bson_t *bson, const char *key, size_t key_length,
                    const struct rbh_filter *filter, bool negate)
{
    bson_t document;

    return bson_append_document_begin(bson, key, key_length, &document)
        && _bson_append_filter(&document, filter, negate)
        && bson_append_document_end(bson, &document);
}
