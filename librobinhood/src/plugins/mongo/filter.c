/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
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
    [RBH_FOP_EXISTS]            = "$exists",
    [RBH_FOP_REGEX]             = NULL, /* This is not a mistake */
    [RBH_FOP_BITS_ANY_SET]      = "$bitsAnySet",
    [RBH_FOP_BITS_ALL_SET]      = "$bitsAllSet",
    [RBH_FOP_BITS_ANY_CLEAR]    = "$bitsAnyClear",
    [RBH_FOP_BITS_ALL_CLEAR]    = "$bitsAllClear",
    [RBH_FOP_AND]               = "$and",
    [RBH_FOP_OR]                = "$or",
    [RBH_FOP_ELEMMATCH]         = "$elemMatch",
};

static const char * const NEGATED_FOP2STR[] = {
    [RBH_FOP_EQUAL]             = "$ne",
    [RBH_FOP_STRICTLY_LOWER]    = "$gte",
    [RBH_FOP_LOWER_OR_EQUAL]    = "$gt",
    [RBH_FOP_STRICTLY_GREATER]  = "$lte",
    [RBH_FOP_GREATER_OR_EQUAL]  = "$lt",
    [RBH_FOP_IN]                = "$nin",
    [RBH_FOP_EXISTS]            = "$exists", /* Negation is "exists" = false */
    [RBH_FOP_REGEX]             = "$not", /* This is not a mistake */
    [RBH_FOP_BITS_ANY_SET]      = "$bitsAllClear",
    [RBH_FOP_BITS_ALL_SET]      = "$bitsAnyClear",
    [RBH_FOP_BITS_ANY_CLEAR]    = "$bitsAllSet",
    [RBH_FOP_BITS_ALL_CLEAR]    = "$bitsAnySet",
    [RBH_FOP_AND]               = "$or",
    [RBH_FOP_OR]                = "$and",
    [RBH_FOP_ELEMMATCH]         = "$not",
};

static const char *
fop2str(enum rbh_filter_operator op, bool negate)
{
    return negate ? NEGATED_FOP2STR[op] : FOP2STR[op];
}

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
                         const struct rbh_filter_field *field, uint32_t u32,
                         bool negate)
{
    const struct rbh_filter LOWER = {
        .op = op,
        .compare = {
            .field = *field,
            .value = {
                .type = RBH_VT_INT32,
                .int32 = u32,
            },
        },
    };
    const struct rbh_filter POSITIVE = {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = *field,
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
                           const struct rbh_filter_field *field, uint32_t u32,
                           bool negate)
{
    const struct rbh_filter GREATER = {
        .op = op,
        .compare = {
            .field = *field,
            .value = {
                .type = RBH_VT_INT32,
                .int32 = u32,
            },
        },
    };
    const struct rbh_filter NEGATIVE = {
        .op = RBH_FOP_STRICTLY_LOWER,
        .compare = {
            .field = *field,
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
                         const struct rbh_filter_field *field, uint64_t u64,
                         bool negate)
{
    const struct rbh_filter LOWER = {
        .op = op,
        .compare = {
            .field = *field,
            .value = {
                .type = RBH_VT_INT64,
                .int64 = u64,
            },
        },
    };
    const struct rbh_filter POSITIVE = {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = *field,
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
                           const struct rbh_filter_field *field, uint64_t u64,
                           bool negate)
{
    const struct rbh_filter GREATER = {
        .op = op,
        .compare = {
            .field = *field,
            .value = {
                .type = RBH_VT_INT64,
                .int64 = u64,
            },
        },
    };
    const struct rbh_filter NEGATIVE = {
        .op = RBH_FOP_STRICTLY_LOWER,
        .compare = {
            .field = *field,
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
bson_append_comparison(bson_t *bson, const char *key, size_t key_length,
                       enum rbh_filter_operator op,
                       const struct rbh_value *value, bool negate)
{
    struct rbh_value tmp;
    bson_t document;

    switch (op) {
    case RBH_FOP_REGEX:
        /* The regex operator is tricky: $not and $regex are not compatible.
         *
         *
         * The workaround is to not use the $regex operator and replace it
         * with the "/pattern/" syntax.
         *
         * Example:
         *
         *      (key =~ pattern) <=> {key: /pattern/}}
         *     !(key =~ pattern) <=> {key: {$not: /pattern/}}
         *
         * Which is why NEGATED_FOP2STR[RBH_FOP_REGEX] is defined as "$not",
         * and also why FOP2STR[RBH_FOP_REGEX] is defined as NULL (because it
         * should never be used).
         *
         * XXX: this is fixed in mongo 4.0.7
         */

        /* The trick only works if `value' is a regex. This is ensured by
         * rbh_filter_validate()... But it cannot hurt to check.
         */
        assert(value->type == RBH_VT_REGEX);
        if (!negate)
            return bson_append_rbh_value(bson, key, key_length, value);
        break;
    case RBH_FOP_EXISTS:
        /* The exists operator doesn't have a negation in mongo's list of
         * operators, `{$not: {$exists: true}}` is simply `{$exists: false}`.
         */
        assert(value->type == RBH_VT_BOOLEAN);
        tmp.type = value->type;
        tmp.boolean = value->boolean ^ negate;
        value = &tmp;
        break;
    default:
        break;
    }

    return bson_append_document_begin(bson, key, key_length, &document)
        && BSON_APPEND_RBH_VALUE(&document, fop2str(op, negate), value)
        && bson_append_document_end(bson, &document);
}

#define BSON_APPEND_COMPARISON(bson, key, op, value, negate) \
    bson_append_comparison(bson, key, strlen(key), op, value, negate)

#define XATTR_ONSTACK_LENGTH 128

static bool
bson_append_comparison_filter(bson_t *bson, const struct rbh_filter *filter,
                              bool negate)
{
    const struct rbh_filter_field *field = &filter->compare.field;
    const struct rbh_value *value = &filter->compare.value;
    enum rbh_filter_operator op = filter->op;
    char onstack[XATTR_ONSTACK_LENGTH];
    char *buffer = onstack;
    const char *key;
    bool success;

    /* Mongo does not support unsigned integers, but we can emulate it */
    switch (op) {
    case RBH_FOP_STRICTLY_LOWER:
    case RBH_FOP_LOWER_OR_EQUAL:
        switch (value->type) {
        case RBH_VT_UINT32:
            return bson_append_uint32_lower(bson, op, field, value->uint32,
                                            negate);
        case RBH_VT_UINT64:
            return bson_append_uint64_lower(bson, op, field, value->uint64,
                                            negate);
        default:
            break;
        }
        break;
    case RBH_FOP_STRICTLY_GREATER:
    case RBH_FOP_GREATER_OR_EQUAL:
        switch (value->type) {
        case RBH_VT_UINT32:
            return bson_append_uint32_greater(bson, op, field, value->uint32,
                                              negate);
        case RBH_VT_UINT64:
            return bson_append_uint64_greater(bson, op, field, value->uint64,
                                              negate);
        default:
            break;
        }
        break;
    default:
        break;
    }

    key = field2str(field, &buffer, sizeof(onstack));
    if (key == NULL)
        return false;

    success = BSON_APPEND_COMPARISON(bson, key, op, value, negate);
    if (buffer != onstack)
        free(buffer);
    return success;
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
bson_append_array_filter(bson_t *bson, const struct rbh_filter *filter,
                         bool negate)
{
    const struct rbh_filter_field *field = &filter->array.field;
    char onstack[XATTR_ONSTACK_LENGTH];
    char *buffer = onstack;
    bson_t *negate_subdoc;
    bson_t subdocuments;
    bson_t document;
    const char *key;

    key = field2str(field, &buffer, sizeof(onstack));
    if (key == NULL)
        return false;

    if (!bson_append_document_begin(bson, key, strlen(key), &document))
        return false;

    if (negate) {
        negate_subdoc = bson_new();
        if (negate_subdoc == NULL)
            return false;

        if (!bson_append_document_begin(&document, fop2str(filter->op, negate),
                                        strlen(fop2str(filter->op, negate)),
                                        negate_subdoc))
            return false;
    } else {
        negate_subdoc = &document;
    }

    if (!bson_append_document_begin(negate_subdoc, fop2str(filter->op, false),
                                    strlen(fop2str(filter->op, false)),
                                    &subdocuments))
        return false;

    for (uint32_t i = 0; i < filter->array.count; i++) {
        const struct rbh_filter *subfilter = filter->array.filters[i];

        if (!BSON_APPEND_RBH_VALUE(&subdocuments,
                                   fop2str(subfilter->op, negate),
                                   &subfilter->compare.value))
            return false;
    }

    if (!bson_append_document_end(negate_subdoc, &subdocuments))
        return false;

    if (negate) {
        if (!bson_append_document_end(&document, negate_subdoc))
            return false;

        bson_destroy(negate_subdoc);
    }

    return bson_append_document_end(bson, &document);
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
    else if (rbh_is_array_operator(filter->op))
        return bson_append_array_filter(bson, filter, negate);
    else if (rbh_is_get_operator(filter->op))
        return bson_append_comparison_filter(bson, filter->get.filter, negate);
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
