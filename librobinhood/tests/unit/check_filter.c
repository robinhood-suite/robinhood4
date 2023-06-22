/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include <sys/stat.h>

#include "robinhood/filter.h"
#include "robinhood/statx.h"

#include "check-compat.h"
#include "check_macros.h"
#include "utils.h"

/*----------------------------------------------------------------------------*
 |                               tests helpers                                |
 *----------------------------------------------------------------------------*/

static const char *
filter_operator2str(enum rbh_filter_operator op)
{
    switch (op) {
    case RBH_FOP_EQUAL:
        return "RBH_FOP_EQUAL";
    case RBH_FOP_STRICTLY_LOWER:
        return "RBH_FOP_STRICTLY_LOWER";
    case RBH_FOP_LOWER_OR_EQUAL:
        return "RBH_FOP_LOWER_OR_EQUAL";
    case RBH_FOP_STRICTLY_GREATER:
        return "RBH_FOP_STRICTLY_GREATER";
    case RBH_FOP_GREATER_OR_EQUAL:
        return "RBH_FOP_GREATER_OR_EQUAL";
    case RBH_FOP_IN:
        return "RBH_FOP_IN";
    case RBH_FOP_REGEX:
        return "RBH_FOP_REGEX";
    case RBH_FOP_BITS_ANY_SET:
        return "RBH_FOP_BITS_ANY_SET";
    case RBH_FOP_BITS_ALL_SET:
        return "RBH_FOP_BITS_ALL_SET";
    case RBH_FOP_BITS_ANY_CLEAR:
        return "RBH_FOP_BITS_ANY_CLEAR";
    case RBH_FOP_BITS_ALL_CLEAR:
        return "RBH_FOP_BITS_ALL_CLEAR";
    case RBH_FOP_AND:
        return "RBH_FOP_AND";
    case RBH_FOP_OR:
        return "RBH_FOP_OR";
    case RBH_FOP_NOT:
        return "RBH_FOP_NOT";
    case RBH_FOP_EXISTS:
        return "RBH_FOP_EXISTS";
    default:
        return "unknown";
    }
}

#define _ck_assert_filter_operator(X, OP, Y) do { \
    ck_assert_msg((X) OP (Y), "%s is %s, %s is %s", \
                  #X, filter_operator2str(X), #Y, filter_operator2str(Y)); \
} while (0)

#define ck_assert_filter_operator_eq(X, Y) _ck_assert_filter_operator(X, ==, Y)

static const char *
fsentry_property2str(enum rbh_fsentry_property property)
{
    switch (property) {
    case RBH_FP_ID:
        return "RBH_FP_ID";
    case RBH_FP_PARENT_ID:
        return "RBH_FP_PARENT_ID";
    case RBH_FP_NAME:
        return "RBH_FP_NAME";
    case RBH_FP_SYMLINK:
        return "RBH_FP_SYMLINK";
    case RBH_FP_STATX:
        return "RBH_FP_STATX";
    case RBH_FP_NAMESPACE_XATTRS:
        return "RBH_FP_NAMESPACE_XATTRS";
    case RBH_FP_INODE_XATTRS:
        return "RBH_FP_INODE_XATTRS";
    }
    return "unknown";
}

static const char *
statx_field2str(unsigned int field)
{
    switch (field) {
    case RBH_STATX_TYPE:
        return "RBH_STATX_TYPE";
    case RBH_STATX_MODE:
        return "RBH_STATX_MODE";
    case RBH_STATX_NLINK:
        return "RBH_STATX_NLINK";
    case RBH_STATX_UID:
        return "RBH_STATX_UID";
    case RBH_STATX_GID:
        return "RBH_STATX_GID";
    case RBH_STATX_ATIME_SEC:
        return "RBH_STATX_ATIME_SEC";
    case RBH_STATX_MTIME_SEC:
        return "RBH_STATX_MTIME_SEC";
    case RBH_STATX_CTIME_SEC:
        return "RBH_STATX_CTIME_SEC";
    case RBH_STATX_INO:
        return "RBH_STATX_INO";
    case RBH_STATX_SIZE:
        return "RBH_STATX_SIZE";
    case RBH_STATX_BLOCKS:
        return "RBH_STATX_BLOCKS";
    case RBH_STATX_BTIME_SEC:
        return "RBH_STATX_BTIME_SEC";
    case RBH_STATX_BLKSIZE:
        return "RBH_STATX_BLKSIZE";
    case RBH_STATX_ATTRIBUTES:
        return "RBH_STATX_ATTRIBUTES";
    case RBH_STATX_ATIME_NSEC:
        return "RBH_STATX_ATIME_NSEC";
    case RBH_STATX_BTIME_NSEC:
        return "RBH_STATX_BTIME_NSEC";
    case RBH_STATX_CTIME_NSEC:
        return "RBH_STATX_CTIME_NSEC";
    case RBH_STATX_MTIME_NSEC:
        return "RBH_STATX_MTIME_NSEC";
    case RBH_STATX_RDEV_MAJOR:
        return "RBH_STATX_RDEV_MAJOR";
    case RBH_STATX_RDEV_MINOR:
        return "RBH_STATX_RDEV_MINOR";
    case RBH_STATX_DEV_MAJOR:
        return "RBH_STATX_DEV_MAJOR";
    case RBH_STATX_DEV_MINOR:
        return "RBH_STATX_DEV_MINOR";
    }
    return "unknown";
}

#define _ck_assert_fsentry_property(X, OP, Y) \
    ck_assert_msg((X) OP (Y), "%s is %s, %s is %s", \
                  #X, fsentry_property2str(X), \
                  #Y, fsentry_property2str(Y))

#define _ck_assert_statx_field(X, OP, Y) \
    ck_assert_msg((X) OP (Y), "%s is %s, %s is %s", \
                  #X, statx_field2str(X), \
                  #Y, statx_field2str(Y))

#define ck_assert_filter_field_eq(X, Y) do { \
    _ck_assert_fsentry_property((X)->fsentry, ==, (Y)->fsentry); \
    switch ((X)->fsentry) { \
    case RBH_FP_ID: \
    case RBH_FP_PARENT_ID: \
    case RBH_FP_NAME: \
    case RBH_FP_SYMLINK: \
        break; \
    case RBH_FP_STATX: \
        _ck_assert_statx_field((X)->statx, ==, (Y)->statx); \
        break; \
    case RBH_FP_NAMESPACE_XATTRS: \
    case RBH_FP_INODE_XATTRS: \
        ck_assert_pstr_eq((X)->xattr, (Y)->xattr); \
        break; \
    } \
} while (0)

#define ck_assert_comparison_filter_eq(X, Y) do { \
    ck_assert_filter_field_eq(&(X)->compare.field, &(Y)->compare.field); \
    ck_assert_value_eq(&(X)->compare.value, &(Y)->compare.value); \
} while (0)

#define ck_assert_filter_eq(X, Y) do { \
    if ((X) == NULL || (Y) == NULL) { \
        ck_assert_ptr_eq(X, Y); \
        break; \
    } \
    ck_assert_filter_operator_eq((X)->op, (Y)->op); \
    if (rbh_is_comparison_operator((X)->op)) { \
        ck_assert_comparison_filter_eq(X, Y); \
    } else { \
        ck_assert_uint_eq((X)->logical.count, (Y)->logical.count); \
        /* Recursion has to be done manually */ \
    } \
} while (0)


/*----------------------------------------------------------------------------*
 |                          rbh_filter_compare_new()                          |
 *----------------------------------------------------------------------------*/

START_TEST(rfcn_basic)
{
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID,
            },
            .value = {
                .type = RBH_VT_BINARY,
                .binary = {
                    .data = "abcdefghijklmnop",
                    .size = 16,
                },
            },
        },
    };
    struct rbh_filter *filter;

    filter = rbh_filter_compare_new(FILTER.op, &FILTER.compare.field,
                                    &FILTER.compare.value);
    ck_assert_ptr_nonnull(filter);

    ck_assert_filter_eq(filter, &FILTER);
    free(filter);
}
END_TEST

START_TEST(rfcn_bad_operator)
{
    const struct rbh_filter_field FIELD = {
        .fsentry = RBH_FP_ID,
    };
    const struct rbh_value VALUE = {};

    errno = 0;
    ck_assert_ptr_null(rbh_filter_compare_new(-1, &FIELD, &VALUE));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfcn_in_without_sequence)
{
    const struct rbh_filter_field FIELD = {
        .fsentry = RBH_FP_ID,
    };
    const struct rbh_value VALUE = {
        .type = RBH_VT_UINT32,
    };

    errno = 0;
    ck_assert_ptr_null(rbh_filter_compare_new(RBH_FOP_IN, &FIELD, &VALUE));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfcn_regex_without_regex)
{
    const struct rbh_filter_field FIELD = {
        .fsentry = RBH_FP_ID,
    };
    const struct rbh_value VALUE = {
        .type = RBH_VT_UINT32,
    };

    errno = 0;
    ck_assert_ptr_null(rbh_filter_compare_new(RBH_FOP_REGEX, &FIELD, &VALUE));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

static const enum rbh_filter_operator BITWISE_OPS[] = {
    RBH_FOP_BITS_ANY_SET,
    RBH_FOP_BITS_ALL_SET,
    RBH_FOP_BITS_ANY_CLEAR,
    RBH_FOP_BITS_ALL_CLEAR,
};

START_TEST(rfcn_bitwise_without_integer)
{
    const struct rbh_filter_field FIELD = {
        .fsentry = RBH_FP_ID,
    };
    const struct rbh_value VALUE = {
        .type = RBH_VT_STRING,
    };

    errno = 0;
    ck_assert_ptr_null(rbh_filter_compare_new(BITWISE_OPS[_i], &FIELD, &VALUE));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_filter_and_new()                            |
 *----------------------------------------------------------------------------*/

static const struct rbh_filter COMPARISONS[] = {
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID,
            },
            .value = {
                .type = RBH_VT_BINARY,
                .binary = {
                    .size = 16,
                    .data = "abcdefghijklmnop",
                },
            },
        },
    },
    {
        .op = RBH_FOP_STRICTLY_LOWER,
        .compare = {
            .field = {
                .fsentry = RBH_FP_PARENT_ID,
            },
            .value = {
                .type = RBH_VT_UINT32,
                .uint32 = INT32_MAX,
            },
        },
    },
    {
        .op = RBH_FOP_LOWER_OR_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_ATIME_SEC,
            },
            .value = {
                .type = RBH_VT_UINT64,
                .uint64 = UINT64_MAX,
            },
        },
    },
    {
        .op = RBH_FOP_STRICTLY_GREATER,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_MTIME_SEC,
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = INT32_MAX,
            }
        },
    },
    {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_CTIME_SEC,
            },
            .value = {
                .type = RBH_VT_INT64,
                .int64 = INT64_MIN,
            },
        },
    },
    {
        .op = RBH_FOP_IN,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_TYPE,
            },
            .value = {
                .type = RBH_VT_SEQUENCE,
                .sequence = {
                    .values = NULL,
                    .count = 0,
                },
            },
        },
    },
    {
        .op = RBH_FOP_REGEX,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAME,
            },
            .value = {
                .type = RBH_VT_REGEX,
                .regex = {
                    .string = "abcdefg",
                    .options = 0,
                },
            },
        },
    },
    {
        .op = RBH_FOP_BITS_ANY_SET,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_UID,
            },
            .value = {
                .type = RBH_VT_UINT32,
                .uint32 = UINT32_MAX,
            },
        },
    },
    {
        .op = RBH_FOP_BITS_ALL_SET,
        .compare = {
            .field = {
               .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_INO,
            },
            .value = {
                .type = RBH_VT_UINT64,
                .uint64 = UINT64_MAX,
            },
        },
    },
    {
        .op = RBH_FOP_BITS_ANY_CLEAR,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_GID,
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = INT32_MIN,
            },
        },
    },
    {
        .op = RBH_FOP_BITS_ALL_CLEAR,
        .compare = {
            .field = {
               .fsentry = RBH_FP_STATX,
               .statx = RBH_STATX_SIZE,
            },
            .value = {
                .type = RBH_VT_INT64,
                .int64 = INT64_MIN,
            },
        },
    },
    /* The filters above should cover all the possible operators.
     * The filters below should cover all the possible fields (not already
     * covered above).
     */
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_MODE,
            },
            .value = {
                .type = RBH_VT_UINT32,
                .uint32 = S_IFREG,
            },
        },
    },
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_NLINK,
            },
            .value = {
                .type = RBH_VT_UINT32,
                .uint32 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_BLOCKS,
            },
            .value = {
                .type = RBH_VT_UINT64,
                .uint64 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_BTIME_SEC,
            },
            .value = {
                .type = RBH_VT_INT64,
                .int64 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_BLKSIZE,
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_BITS_ALL_SET,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_ATTRIBUTES,
            },
            .value = {
                .type = RBH_VT_INT64,
                .int64 = RBH_STATX_ATTR_APPEND | RBH_STATX_ATTR_COMPRESSED,
            },
        },
    },
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_ATIME_NSEC,
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_BTIME_NSEC,
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_CTIME_NSEC,
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_MTIME_NSEC,
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_RDEV_MAJOR,
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_RDEV_MINOR,
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_DEV_MAJOR,
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_GREATER_OR_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_DEV_MINOR,
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAMESPACE_XATTRS,
                .xattr = NULL,
            },
            .value = {
                .type = RBH_VT_MAP,
                .map = {
                    .count = 0,
                },
            },
        },
    },
    {
            .op = RBH_FOP_EXISTS,
        .compare = {
            .field = {
                .fsentry = RBH_FP_INODE_XATTRS,
                .xattr = "abcdefg",
            },
            .value = {
                .type = RBH_VT_BOOLEAN,
                .boolean = true,
            },
        },
    },
    {
        .op = RBH_FOP_REGEX,
        .compare = {
            .field = {
                .fsentry = RBH_FP_NAMESPACE_XATTRS,
                .xattr = "path",
            },
            .value = {
                .type = RBH_VT_REGEX,
                .regex = {
                    .string = "abcdefg",
                    .options = 0,
                },
            },
        },
    },
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_INODE_XATTRS,
                .xattr = "test",
            },
            .value = {
                .type = RBH_VT_INT32,
                .int32 = 0,
            },
        },
    },
    {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_INODE_XATTRS,
                .xattr = NULL,
            },
            .value = {
                .type = RBH_VT_MAP,
                .map = {
                    .count = 0,
                },
            },
        },
    },
};

START_TEST(rfan_basic)
{
    const struct rbh_filter *filters[ARRAY_SIZE(COMPARISONS) + 1];
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_AND,
        .logical = {
            .filters = filters,
            .count = ARRAY_SIZE(filters),
        },
    };
    struct rbh_filter *filter;

    filters[0] = NULL;
    for (size_t i = 0; i < ARRAY_SIZE(COMPARISONS); i++)
        filters[i + 1] = &COMPARISONS[i];

    filter = rbh_filter_and_new(filters, ARRAY_SIZE(filters));
    ck_assert_ptr_nonnull(filter);

    ck_assert_filter_eq(filter, &FILTER);
    for (size_t i = 0; i < filter->logical.count; i++)
        ck_assert_filter_eq(filter->logical.filters[i], filters[i]);

    free(filter);
}
END_TEST

START_TEST(rfan_zero)
{
    errno = 0;
    ck_assert_ptr_null(rbh_filter_and_new(NULL, 0));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_filter_or_new()                             |
 *----------------------------------------------------------------------------*/

/* The underlying implementation of filter_or() is the same as filter_and()'s.
 * There is no need to test is extensively.
 */
START_TEST(rfon_basic)
{
    const struct rbh_filter * const FILTERS[3] = {};
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_OR,
        .logical = {
            .filters = FILTERS,
            .count = ARRAY_SIZE(FILTERS),
        },
    };
    struct rbh_filter *filter;

    filter = rbh_filter_or_new(FILTERS, ARRAY_SIZE(FILTERS));
    ck_assert_ptr_nonnull(filter);

    ck_assert_filter_eq(filter, &FILTER);
    free(filter);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_filter_not_new()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rfnn_basic)
{
    const struct rbh_filter * const FILTERS[1] = {};
    const struct rbh_filter NEGATED = {
        .op = RBH_FOP_NOT,
        .logical = {
            .filters = FILTERS,
            .count = 1,
        },
    };
    struct rbh_filter *filter;

    filter = rbh_filter_not_new(FILTERS[0]);
    ck_assert_ptr_nonnull(filter);

    ck_assert_filter_eq(filter, &NEGATED);
    free(filter);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_filter_validate()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rfv_null_filter)
{
    ck_assert_int_eq(rbh_filter_validate(NULL), 0);
}
END_TEST

START_TEST(rfv_not_null_filter)
{
    const struct rbh_filter *FILTER_NULL = NULL;
    const struct rbh_filter FILTER_NOT_NULL = {
        .op = RBH_FOP_NOT,
        .logical = {
            .filters = &FILTER_NULL,
            .count = 1,
        },
    };

    ck_assert_int_eq(rbh_filter_validate(&FILTER_NOT_NULL), 0);
}
END_TEST

START_TEST(rfv_bad_operator)
{
    const struct rbh_filter INVALID = {
        .op = -1,
    };

    errno = 0;
    ck_assert_int_eq(rbh_filter_validate(&INVALID), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

/* The internal function op_matches_value() is already tested by:
 *   - rfcn_bad_operator;
 *   - rfcn_in_without_sequence;
 *   - rfcn_regex_without_regex;
 *   - rfcn_bitwise_without_integer;
 *
 * Here we just check that when the operator does not match the value,
 * rbh_filter_validate() does fail.
 */
START_TEST(rfv_op_does_not_match_value)
{
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_REGEX,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID,
            },
            .value = {
                .type = RBH_VT_INT32,
            },
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_filter_validate(&FILTER), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfv_valid_comparison)
{
    ck_assert_int_eq(rbh_filter_validate(&COMPARISONS[_i]), 0);
}
END_TEST

START_TEST(rfv_bad_fsentry_field)
{
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID | RBH_FP_PARENT_ID,
            },
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_filter_validate(&FILTER), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfv_bad_statx_field)
{
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_STATX,
                .statx = RBH_STATX_TYPE | RBH_STATX_MODE,
            },
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_filter_validate(&FILTER), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfv_empty_logical)
{
    const struct rbh_filter EMPTY_LOGICAL = {
        .op = RBH_FOP_AND,
        .logical = {
            .count = 0,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_filter_validate(&EMPTY_LOGICAL), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfv_logical_with_invalid)
{
    const struct rbh_filter INVALID = {
        .op = -1,
    };
    const struct rbh_filter *filters = &INVALID;
    const struct rbh_filter LOGICAL = {
        .op = RBH_FOP_LOGICAL_MIN,
        .logical = {
            .filters = &filters,
            .count = 1,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_filter_validate(&LOGICAL), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfv_many_not)
{
    const struct rbh_filter NOT_FILTER = {
        .op = RBH_FOP_NOT,
        .logical = {
            .count = 2,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_filter_validate(&NOT_FILTER), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rfv_single_and)
{
    const struct rbh_filter *filter = &COMPARISONS[0];
    const struct rbh_filter AND_FILTER = {
        .op = RBH_FOP_AND,
        .logical = {
            .filters = &filter,
            .count = 1,
        },
    };

    ck_assert_int_eq(rbh_filter_validate(&AND_FILTER), 0);
}
END_TEST

START_TEST(rfv_many_and)
{
    const struct rbh_filter * const FILTERS[2] = {};
    const struct rbh_filter AND_FILTER = {
        .op = RBH_FOP_AND,
        .logical = {
            .filters = FILTERS,
            .count = ARRAY_SIZE(FILTERS),
        },
    };

    ck_assert_int_eq(rbh_filter_validate(&AND_FILTER), 0);
}
END_TEST

START_TEST(rfv_many_or)
{
    const struct rbh_filter * const FILTERS[2] = {};
    const struct rbh_filter OR_FILTER = {
        .op = RBH_FOP_OR,
        .logical = {
            .filters = FILTERS,
            .count = ARRAY_SIZE(FILTERS),
        },
    };

    ck_assert_int_eq(rbh_filter_validate(&OR_FILTER), 0);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                             rbh_filter_clone()                             |
 *----------------------------------------------------------------------------*/

START_TEST(rfc_basic)
{
    const struct rbh_filter FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID,
            },
            .value = {
                .type = RBH_VT_BINARY,
                .binary = {
                    .data = "abcdefghijklmnop",
                    .size = 16,
                },
            },
        },
    };
    struct rbh_filter *clone;

    clone = rbh_filter_clone(&FILTER);
    ck_assert_ptr_nonnull(clone);
    ck_assert_filter_eq(clone, &FILTER);
    ck_assert_ptr_ne(clone, &FILTER);
    free(clone);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("filter interface");

    tests = tcase_create("rbh_filter_compare_new");
    tcase_add_test(tests, rfcn_basic);
    tcase_add_test(tests, rfcn_bad_operator);
    tcase_add_test(tests, rfcn_in_without_sequence);
    tcase_add_test(tests, rfcn_regex_without_regex);
    tcase_add_loop_test(tests, rfcn_bitwise_without_integer, 0,
                        ARRAY_SIZE(BITWISE_OPS));

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_and_new");
    tcase_add_test(tests, rfan_basic);
    tcase_add_test(tests, rfan_zero);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_or_new");
    tcase_add_test(tests, rfon_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_not_new");
    tcase_add_test(tests, rfnn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_validate");
    tcase_add_test(tests, rfv_null_filter);
    tcase_add_test(tests, rfv_not_null_filter);
    tcase_add_test(tests, rfv_bad_operator);
    tcase_add_test(tests, rfv_op_does_not_match_value);
    tcase_add_loop_test(tests, rfv_valid_comparison, 0,
                        ARRAY_SIZE(COMPARISONS));
    tcase_add_test(tests, rfv_bad_fsentry_field);
    tcase_add_test(tests, rfv_bad_statx_field);
    tcase_add_test(tests, rfv_empty_logical);
    tcase_add_test(tests, rfv_logical_with_invalid);
    tcase_add_test(tests, rfv_many_not);
    tcase_add_test(tests, rfv_single_and);
    tcase_add_test(tests, rfv_many_and);
    tcase_add_test(tests, rfv_many_or);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_filter_clone");
    tcase_add_test(tests, rfc_basic);

    suite_add_tcase(suite, tests);

    return suite;
}

int
main(void)
{
    int number_failed;
    Suite *suite;
    SRunner *runner;

    suite = unit_suite();
    runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
