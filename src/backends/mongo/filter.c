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

#include <assert.h>

#include "robinhood/id.h"
#include "robinhood/filter.h"

#include "mongo.h"

/*----------------------------------------------------------------------------*
 |                          bson_append_rbh_filter()                          |
 *----------------------------------------------------------------------------*/

/* The following helpers should only be used on a valid filter */

static const char * const FOP2STR[] = {
    [RBH_FOP_EQUAL]             = "$eq",
    [RBH_FOP_STRICTLY_LOWER]    = "$lt",
    [RBH_FOP_LOWER_OR_EQUAL]    = "$lte",
    [RBH_FOP_STRICTLY_GREATER]  = "$gt",
    [RBH_FOP_GREATER_OR_EQUAL]  = "$gte",
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
    [RBH_FOP_STRICTLY_LOWER]    = "$gte",
    [RBH_FOP_LOWER_OR_EQUAL]    = "$gt",
    [RBH_FOP_STRICTLY_GREATER]  = "$lte",
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
    [RBH_FF_ID]                 = MFF_ID,
    [RBH_FF_PARENT_ID]          = MFF_NAMESPACE "." MFF_PARENT_ID,
    [RBH_FF_NAME]               = MFF_NAMESPACE "." MFF_NAME,
    [RBH_FF_NAMESPACE_XATTRS]   = MFF_NAMESPACE "." MFF_XATTRS,
    [RBH_FF_INODE_XATTRS]       = MFF_XATTRS,
    [RBH_FF_TYPE]               = MFF_STATX "." MFF_STATX_TYPE,
    [RBH_FF_ATIME]              = MFF_STATX "." MFF_STATX_ATIME "." MFF_STATX_TIMESTAMP_SEC,
    [RBH_FF_CTIME]              = MFF_STATX "." MFF_STATX_CTIME "." MFF_STATX_TIMESTAMP_SEC,
    [RBH_FF_MTIME]              = MFF_STATX "." MFF_STATX_MTIME "." MFF_STATX_TIMESTAMP_SEC,
};

static bool
_bson_append_rbh_filter(bson_t *bson, const struct rbh_filter *filter,
                        bool negate);

/* MongoDB does not handle _unsigned_ integers natively, their support has to
 * be emulated.
 *
 * Filters using unsigned integers have to be converted to ones that only use
 * signed integers. There are 3 different criteria that determine how one filter
 * is converted into the other:
 *   - the type of integer (uint32_t or uint64_t);
 *   - the type of comparison ('<' or '>');
 *   - whether casting from unsigned to signed causes an overflow. [*]
 *
 * Here are 3 examples highlighting the impact of each criteria:
 *
 * {X < (uint64_t)40}            <=> {X >= (int64_t)0 && X < (int64_t)40}
 * {X > (uint32_t)40}            <=> {X <  (int32_t)0 || X > (int32_t)40}
 * {X < (uint32_t)INT32_MAX + 1} <=> {X >= (int32_t)0 || X < (int32_t)INT32_MIN}
 *
 * The following bson_append_uint{32,64}_{lower,greater}() functions handle this
 * conversion.
 *
 * Note that whether the initial comparison operator is strict or not (< or <=)
 * is not considered an important criteria, as it has little impact on the
 * conversion:
 *
 * {X <  (uint32_t)40}           <=> {X >= (int32_t)0 && X <  (int32_t)40}
 * {X <= (uint32_t)40}           <=> {X >= (int32_t)0 && X <= (int32_t)40}
 *
 * [*] signed integer overflow is not part of the C standard, but gcc and clang
 *     both implement it in a way that fits our purpose.
 */

static bool
bson_append_uint32_lower(bson_t *bson, enum rbh_filter_operator op,
                         enum rbh_filter_field field, const char *xattr,
                         uint32_t u32, bool negate)
{
    const struct rbh_filter LOWER = {
        .op = op,
        .compare = {
            .field = field,
            .xattr = xattr,
            .value = {
                .type = RBH_VT_INT32,
                .int32 = u32,
            },
        },
    };
    const struct rbh_filter POSITIVE = {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = field,
            .xattr = xattr,
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    };
    const struct rbh_filter *COMPARISONS[2] = {&LOWER, &POSITIVE};
    const struct rbh_filter FILTER = {
        .op = u32 <= INT32_MAX ? RBH_FOP_AND : RBH_FOP_OR,
        .logical = {
            .filters = COMPARISONS,
            .count = 2,
        },
    };

    assert(op == RBH_FOP_STRICTLY_LOWER || op == RBH_FOP_LOWER_OR_EQUAL);
    return _bson_append_rbh_filter(bson, &FILTER, negate);
}

static bool
bson_append_uint32_greater(bson_t *bson, enum rbh_filter_operator op,
                           enum rbh_filter_field field, const char *xattr,
                           uint32_t u32, bool negate)
{
    const struct rbh_filter GREATER = {
        .op = op,
        .compare = {
            .field = field,
            .xattr = xattr,
            .value = {
                .type = RBH_VT_INT32,
                .int32 = u32,
            },
        },
    };
    const struct rbh_filter NEGATIVE = {
        .op = RBH_FOP_STRICTLY_LOWER,
        .compare = {
            .field = field,
            .xattr = xattr,
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    };
    const struct rbh_filter *COMPARISONS[2] = {&GREATER, &NEGATIVE};
    const struct rbh_filter FILTER = {
        .op = u32 <= INT32_MAX ? RBH_FOP_OR : RBH_FOP_AND,
        .logical = {
            .filters = COMPARISONS,
            .count = 2,
        },
    };

    assert(op == RBH_FOP_STRICTLY_GREATER || op == RBH_FOP_GREATER_OR_EQUAL);
    return _bson_append_rbh_filter(bson, &FILTER, negate);
}

static bool
bson_append_uint64_lower(bson_t *bson, enum rbh_filter_operator op,
                         enum rbh_filter_field field, const char *xattr,
                         uint64_t u64, bool negate)
{
    const struct rbh_filter LOWER = {
        .op = op,
        .compare = {
            .field = field,
            .xattr = xattr,
            .value = {
                .type = RBH_VT_INT64,
                .int64 = u64,
            },
        },
    };
    const struct rbh_filter POSITIVE = {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = field,
            .xattr = xattr,
            .value = {
                .type = RBH_VT_INT64,
                .int64 = 0,
            },
        },
    };
    const struct rbh_filter *COMPARISONS[2] = {&LOWER, &POSITIVE};
    const struct rbh_filter FILTER = {
        .op = u64 <= INT64_MAX ? RBH_FOP_AND : RBH_FOP_OR,
        .logical = {
            .filters = COMPARISONS,
            .count = 2,
        },
    };

    assert(op == RBH_FOP_STRICTLY_LOWER || op == RBH_FOP_LOWER_OR_EQUAL);
    return _bson_append_rbh_filter(bson, &FILTER, negate);
}
static bool
bson_append_uint64_greater(bson_t *bson, enum rbh_filter_operator op,
                           enum rbh_filter_field field, const char *xattr,
                           uint64_t u64, bool negate)
{
    const struct rbh_filter GREATER = {
        .op = op,
        .compare = {
            .field = field,
            .xattr = xattr,
            .value = {
                .type = RBH_VT_INT64,
                .int64 = u64,
            },
        },
    };
    const struct rbh_filter NEGATIVE = {
        .op = RBH_FOP_STRICTLY_LOWER,
        .compare = {
            .field = field,
            .xattr = xattr,
            .value = {
                .type = RBH_VT_INT64,
                .int64 = 0,
            },
        },
    };
    const struct rbh_filter *COMPARISONS[2] = {&GREATER, &NEGATIVE};
    const struct rbh_filter FILTER = {
        .op = u64 <= INT64_MAX ? RBH_FOP_OR : RBH_FOP_AND,
        .logical = {
            .filters = COMPARISONS,
            .count = 2,
        },
    };

    assert(op == RBH_FOP_STRICTLY_GREATER || op == RBH_FOP_GREATER_OR_EQUAL);
    return _bson_append_rbh_filter(bson, &FILTER, negate);
}

static bool
bson_append_comparison_filter(bson_t *bson, const struct rbh_filter *filter,
                              bool negate)
{
    const struct rbh_value *value = &filter->compare.value;
    enum rbh_filter_field field = filter->compare.field;
    const char *xattr = filter->compare.xattr;
    enum rbh_filter_operator op = filter->op;
    char buffer[XATTR_KEYLEN_MAX];
    const char *fieldstr;
    bson_t document;

    switch (field) {
    case RBH_FF_NAMESPACE_XATTRS:
    case RBH_FF_INODE_XATTRS:
        if (xattr) {
            int rc = snprintf(buffer, sizeof(buffer), "%s.%s", FIELD2STR[field],
                              xattr);

            assert(rc >= 0);
            if (rc >= sizeof(buffer))
                /* Errors from bson_append_*() are interpreted as ENOBUFs, as in
                 * `bson' is full... Close enough.
                 */
                return false;
            fieldstr = buffer;
            break;
        } /* else */
        __attribute__((fallthrough));
    default:
        fieldstr = FIELD2STR[field];
    }

    /* Special case a few operators */
    switch (op) {
    /* Mongo does not support unsigned integers, but we can emulate it */
    case RBH_FOP_STRICTLY_LOWER:
    case RBH_FOP_LOWER_OR_EQUAL:
        switch (value->type) {
        case RBH_VT_UINT32:
            return bson_append_uint32_lower(bson, op, field, xattr,
                                            value->uint32, negate);
        case RBH_VT_UINT64:
            return bson_append_uint64_lower(bson, op, field, xattr,
                                            value->uint64, negate);
        default:
            break;
        }
        break;
    case RBH_FOP_STRICTLY_GREATER:
    case RBH_FOP_GREATER_OR_EQUAL:
        switch (value->type) {
        case RBH_VT_UINT32:
            return bson_append_uint32_greater(bson, op, field, xattr,
                                              value->uint32, negate);
        case RBH_VT_UINT64:
            return bson_append_uint64_greater(bson, op, field, xattr,
                                              value->uint64, negate);
        default:
            break;
        }
        break;
    case RBH_FOP_REGEX:
        /* The regex operator is tricky: $not and $regex are not compatible.
         *
         * The workaround is not to use the $regex operator and replace it with
         * the "/pattern/" syntax.
         */
        if (!negate)
            return BSON_APPEND_RBH_VALUE(bson, fieldstr, value);
        break;
    default:
        break;
    }

    return BSON_APPEND_DOCUMENT_BEGIN(bson, fieldstr, &document)
        && BSON_APPEND_RBH_VALUE(&document, fop2str(op, negate), value)
        && bson_append_document_end(bson, &document);
}

static bool
bson_append_logical_filter(bson_t *bson, const struct rbh_filter *filter,
                           bool negate)
{
    bson_t array;

    if (filter->op == RBH_FOP_NOT)
        return _bson_append_rbh_filter(bson, filter->logical.filters[0],
                                       !negate);

    if (!BSON_APPEND_ARRAY_BEGIN(bson, fop2str(filter->op, negate), &array))
        return false;

    for (uint32_t i = 0; i < filter->logical.count; i++) {
        const char *key;
        size_t length;
        char str[16];

        length = bson_uint32_to_string(i, &key, str, sizeof(str));
        if (!bson_append_rbh_filter(&array, key, length,
                                    filter->logical.filters[i], negate))
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
_bson_append_rbh_filter(bson_t *bson, const struct rbh_filter *filter,
                        bool negate)
{
    if (filter == NULL)
        return bson_append_null_filter(bson, negate);

    if (rbh_is_comparison_operator(filter->op))
        return bson_append_comparison_filter(bson, filter, negate);
    return bson_append_logical_filter(bson, filter, negate);
}

bool
bson_append_rbh_filter(bson_t *bson, const char *key, size_t key_length,
                       const struct rbh_filter *filter, bool negate)
{
    bson_t document;

    return bson_append_document_begin(bson, key, key_length, &document)
        && _bson_append_rbh_filter(&document, filter, negate)
        && bson_append_document_end(bson, &document);
}

/*----------------------------------------------------------------------------*
 |                        bson_append_rbh_id_filter()                         |
 *----------------------------------------------------------------------------*/

bool
bson_append_rbh_id_filter(bson_t *bson, const char *key, size_t key_length,
                          const struct rbh_id *id)
{
    return _bson_append_binary(bson, key, key_length, BSON_SUBTYPE_BINARY,
                               id->data, id->size)
        && (id->size != 0 || BSON_APPEND_INT32(bson, "type", BSON_TYPE_NULL));
}
