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
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "robinhood/filter.h"

#include "value.h"
#include "utils.h"

static int
comparison_filter_copy(struct rbh_filter *dest, const struct rbh_filter *src,
                       char **buffer, size_t *bufsize)
{
    size_t size = *bufsize;
    char *data = *buffer;

    /* dest->compare.field */
    dest->compare.field = src->compare.field;

    /* dest->compare.xattr */
    switch (src->compare.field) {
    case RBH_FF_NAMESPACE_XATTRS:
    case RBH_FF_INODE_XATTRS:
        if (src->compare.xattr) {
            size_t length = strlen(src->compare.xattr) + 1;

            assert(size >= length);
            dest->compare.xattr = data;
            data = mempcpy(data, src->compare.xattr, length);
            size -= length;
        }
    default:
        /* Nothing to do */
        break;
    }

    /* dest->compare.value */
    if (value_copy(&dest->compare.value, &src->compare.value, &data, &size)) {
        assert(errno != ENOBUFS);
        return -1;
    }

    *buffer = data;
    *bufsize = size;
    return 0;
}

static int
filter_copy(struct rbh_filter *dest, const struct rbh_filter *src,
            char **buffer, size_t *bufsize);

static int
logical_filter_copy(struct rbh_filter *dest, const struct rbh_filter *src,
                    char **buffer, size_t *bufsize)
{
    const struct rbh_filter **filters;
    size_t size = *bufsize;
    char *data = *buffer;

    /* dest->logical.filters */
    data = ptralign(data, &size, alignof(*filters));
    assert(size >= sizeof(*filters) * src->logical.count);
    filters = (const struct rbh_filter **)data;
    data += sizeof(*filters) * src->logical.count;
    size -= sizeof(*filters) * src->logical.count;

    for (size_t i = 0; i < src->logical.count; i++) {
        struct rbh_filter *filter;

        if (src->logical.filters[i] == NULL) {
            filters[i] = NULL;
            continue;
        }

        data = ptralign(data, &size, alignof(*filter));
        assert(size >= sizeof(*filter));
        filter = (struct rbh_filter *)data;
        data += sizeof(*filter);
        size -= sizeof(*filter);

        if (filter_copy(filter, src->logical.filters[i], &data, &size))
            return -1;

        filters[i] = filter;
    }
    dest->logical.filters = filters;

    /* dest->logical.count */
    dest->logical.count = src->logical.count;

    *buffer = data;
    *bufsize = size;
    return 0;
}

static int
filter_copy(struct rbh_filter *dest, const struct rbh_filter *src,
            char **buffer, size_t *bufsize)
{
    assert(src != NULL);

    /* dest->op */
    dest->op = src->op;

    if (rbh_is_comparison_operator(src->op))
        /* dest->compare */
        return comparison_filter_copy(dest, src, buffer, bufsize);
    return logical_filter_copy(dest, src, buffer, bufsize);
}

static ssize_t __attribute__((pure))
filter_data_size(const struct rbh_filter *filter)
{
    size_t size = 0;

    if (filter == NULL)
        return 0;

    switch (filter->op) {
    case RBH_FOP_COMPARISON_MIN ... RBH_FOP_COMPARISON_MAX:
        switch (filter->compare.field) {
        case RBH_FF_NAMESPACE_XATTRS:
        case RBH_FF_INODE_XATTRS:
            if (filter->compare.xattr)
                size = strlen(filter->compare.xattr) + 1;
            break;
        default:
            break;
        }
        return size + value_data_size(&filter->compare.value);
    case RBH_FOP_LOGICAL_MIN ... RBH_FOP_LOGICAL_MAX:
        size += sizeof(filter) * filter->logical.count;
        for (size_t i = 0; i < filter->logical.count; i++) {
            if (filter->logical.filters[i] == NULL)
                continue;

            size = sizealign(size, alignof(*filter));
            size += sizeof(*filter);
            if (filter_data_size(filter->logical.filters[i]) < 0)
                return -1;
            size += filter_data_size(filter->logical.filters[i]);
        }
        return size;
    }

    errno = EINVAL;
    return -1;
}

static struct rbh_filter *
filter_clone(const struct rbh_filter *filter)
{
    struct rbh_filter *clone;
    size_t size;
    char *data;
    int rc;

    if (filter == NULL)
        return NULL;

    if (filter_data_size(filter) < 0)
        return NULL;
    size = filter_data_size(filter);

    clone = malloc(sizeof(*clone) + size);
    if (clone == NULL)
        return NULL;
    data = (char *)clone + sizeof(*clone);

    rc = filter_copy(clone, filter, &data, &size);
    assert(rc == 0);

    return clone;
}

static bool
op_matches_value(enum rbh_filter_operator op, const struct rbh_value *value)
{
    if (!rbh_is_comparison_operator(op))
        return false;

    switch (op) {
    case RBH_FOP_IN:
        if (value->type != RBH_VT_SEQUENCE)
            return false;
        break;
    case RBH_FOP_REGEX:
        if (value->type != RBH_VT_REGEX)
            return false;
        break;
    case RBH_FOP_BITS_ANY_SET:
    case RBH_FOP_BITS_ALL_SET:
    case RBH_FOP_BITS_ANY_CLEAR:
    case RBH_FOP_BITS_ALL_CLEAR:
        switch (value->type) {
        case RBH_VT_UINT32:
        case RBH_VT_UINT64:
        case RBH_VT_INT32:
        case RBH_VT_INT64:
            break;
        default:
            return false;
        }
        break;
    default:
        break;
    }

    return true;
}

struct rbh_filter *
rbh_filter_compare_new(enum rbh_filter_operator op, enum rbh_filter_field field,
                       const char *xattr, const struct rbh_value *value)
{
    const struct rbh_filter COMPARE = {
        .op = op,
        .compare = {
            .field = field,
            .xattr = xattr,
            .value = *value,
        },
    };

    if (!op_matches_value(op, value)) {
        errno = EINVAL;
        return NULL;
    }

    return filter_clone(&COMPARE);
}

struct rbh_filter *
rbh_filter_compare_binary_new(enum rbh_filter_operator op,
                              enum rbh_filter_field field, const char *data,
                              size_t size)
{
    const struct rbh_value BINARY = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = data,
            .size = size,
        },
    };

    return rbh_filter_compare_new(op, field, NULL, &BINARY);
}

struct rbh_filter *
rbh_filter_compare_uint32_new(enum rbh_filter_operator op,
                              enum rbh_filter_field field, uint32_t uint32)
{
    const struct rbh_value UINT32 = {
        .type = RBH_VT_UINT32,
        .uint32 = uint32,
    };

    return rbh_filter_compare_new(op, field, NULL, &UINT32);
}

struct rbh_filter *
rbh_filter_compare_uint64_new(enum rbh_filter_operator op,
                              enum rbh_filter_field field, uint64_t uint64)
{
    const struct rbh_value UINT64 = {
        .type = RBH_VT_UINT64,
        .uint64 = uint64,
    };

    return rbh_filter_compare_new(op, field, NULL, &UINT64);
}

struct rbh_filter *
rbh_filter_compare_int32_new(enum rbh_filter_operator op,
                             enum rbh_filter_field field, int32_t int32)
{
    const struct rbh_value INT32 = {
        .type = RBH_VT_INT32,
        .int32 = int32,
    };

    return rbh_filter_compare_new(op, field, NULL, &INT32);
}

struct rbh_filter *
rbh_filter_compare_int64_new(enum rbh_filter_operator op,
                             enum rbh_filter_field field, int64_t int64)
{
    const struct rbh_value INT64 = {
        .type = RBH_VT_INT64,
        .int64 = int64,
    };

    return rbh_filter_compare_new(op, field, NULL, &INT64);
}

struct rbh_filter *
rbh_filter_compare_string_new(enum rbh_filter_operator op,
                              enum rbh_filter_field field, const char *string)
{
    const struct rbh_value STRING = {
        .type = RBH_VT_STRING,
        .string = string,
    };

    return rbh_filter_compare_new(op, field, NULL, &STRING);
}

struct rbh_filter *
rbh_filter_compare_regex_new(enum rbh_filter_operator op,
                             enum rbh_filter_field field, const char *regex,
                             unsigned int regex_options)
{
    const struct rbh_value REGEX = {
        .type = RBH_VT_REGEX,
        .regex = {
            .string = regex,
            .options = regex_options,
        },
    };

    return rbh_filter_compare_new(op, field, NULL, &REGEX);
}

struct rbh_filter *
rbh_filter_compare_sequence_new(enum rbh_filter_operator op,
                                enum rbh_filter_field field,
                                const struct rbh_value values[], size_t count)
{
    const struct rbh_value SEQUENCE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = values,
            .count = count,
        },
    };

    return rbh_filter_compare_new(op, field, NULL, &SEQUENCE);
}

struct rbh_filter *
rbh_filter_compare_map_new(enum rbh_filter_operator op,
                           enum rbh_filter_field field,
                           const struct rbh_value_pair pairs[], size_t count)
{
    const struct rbh_value MAP = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = pairs,
            .count = count,
        },
    };

    return rbh_filter_compare_new(op, field, NULL, &MAP);
}

struct rbh_filter *
rbh_filter_compare_xattr_binary_new(enum rbh_filter_operator op,
                                    const char *xattr, const char *data,
                                    size_t size)
{
    const struct rbh_value BINARY = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = data,
            .size = size,
        },
    };

    return rbh_filter_compare_new(op, RBH_FF_INODE_XATTRS, xattr, &BINARY);
}

struct rbh_filter *
rbh_filter_compare_xattr_uint32_new(enum rbh_filter_operator op,
                                    const char *xattr, uint32_t uint32)
{
    const struct rbh_value UINT32 = {
        .type = RBH_VT_UINT32,
        .uint32 = uint32,
    };

    return rbh_filter_compare_new(op, RBH_FF_INODE_XATTRS, xattr, &UINT32);
}

struct rbh_filter *
rbh_filter_compare_xattr_uint64_new(enum rbh_filter_operator op,
                                    const char *xattr, uint64_t uint64)
{
    const struct rbh_value UINT64 = {
        .type = RBH_VT_UINT64,
        .uint64 = uint64,
    };

    return rbh_filter_compare_new(op, RBH_FF_INODE_XATTRS, xattr, &UINT64);
}

struct rbh_filter *
rbh_filter_compare_xattr_int32_new(enum rbh_filter_operator op,
                                   const char *xattr, int32_t int32)
{
    const struct rbh_value INT32 = {
        .type = RBH_VT_INT32,
        .int32 = int32,
    };

    return rbh_filter_compare_new(op, RBH_FF_INODE_XATTRS, xattr, &INT32);
}

struct rbh_filter *
rbh_filter_compare_xattr_int64_new(enum rbh_filter_operator op,
                                   const char *xattr, int64_t int64)
{
    const struct rbh_value INT64 = {
        .type = RBH_VT_INT64,
        .int64 = int64,
    };

    return rbh_filter_compare_new(op, RBH_FF_INODE_XATTRS, xattr, &INT64);
}

struct rbh_filter *
rbh_filter_compare_xattr_string_new(enum rbh_filter_operator op,
                                    const char *xattr, const char *string)
{
    const struct rbh_value STRING = {
        .type = RBH_VT_STRING,
        .string = string,
    };

    return rbh_filter_compare_new(op, RBH_FF_INODE_XATTRS, xattr, &STRING);
}

struct rbh_filter *
rbh_filter_compare_xattr_regex_new(enum rbh_filter_operator op,
                                   const char *xattr, const char *regex,
                                   unsigned int regex_options)
{
    const struct rbh_value REGEX = {
        .type = RBH_VT_REGEX,
        .regex = {
            .string = regex,
            .options = regex_options,
        },
    };

    return rbh_filter_compare_new(op, RBH_FF_INODE_XATTRS, xattr, &REGEX);
}

struct rbh_filter *
rbh_filter_compare_xattr_sequence_new(enum rbh_filter_operator op,
                                      const char *xattr,
                                      const struct rbh_value values[],
                                      size_t count)
{
    const struct rbh_value SEQUENCE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = values,
            .count = count,
        },
    };

    return rbh_filter_compare_new(op, RBH_FF_INODE_XATTRS, xattr, &SEQUENCE);
}

struct rbh_filter *
rbh_filter_compare_xattr_map_new(enum rbh_filter_operator op, const char *xattr,
                                 const struct rbh_value_pair pairs[],
                                 size_t count)
{
    const struct rbh_value MAP = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = pairs,
            .count = count,
        },
    };

    return rbh_filter_compare_new(op, RBH_FF_INODE_XATTRS, xattr, &MAP);
}

struct rbh_filter *
rbh_filter_compare_ns_xattr_binary_new(enum rbh_filter_operator op,
                                       const char *xattr, const char *data,
                                       size_t size)
{
    const struct rbh_value BINARY = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = data,
            .size = size,
        },
    };

    return rbh_filter_compare_new(op, RBH_FF_NAMESPACE_XATTRS, xattr, &BINARY);
}

struct rbh_filter *
rbh_filter_compare_ns_xattr_uint32_new(enum rbh_filter_operator op,
                                       const char *xattr, uint32_t uint32)
{
    const struct rbh_value UINT32 = {
        .type = RBH_VT_UINT32,
        .uint32 = uint32,
    };

    return rbh_filter_compare_new(op, RBH_FF_NAMESPACE_XATTRS, xattr, &UINT32);
}

struct rbh_filter *
rbh_filter_compare_ns_xattr_uint64_new(enum rbh_filter_operator op,
                                       const char *xattr, uint64_t uint64)
{
    const struct rbh_value UINT64 = {
        .type = RBH_VT_UINT64,
        .uint64 = uint64,
    };

    return rbh_filter_compare_new(op, RBH_FF_NAMESPACE_XATTRS, xattr, &UINT64);
}

struct rbh_filter *
rbh_filter_compare_ns_xattr_int32_new(enum rbh_filter_operator op,
                                      const char *xattr, int32_t int32)
{
    const struct rbh_value INT32 = {
        .type = RBH_VT_INT32,
        .int32 = int32,
    };

    return rbh_filter_compare_new(op, RBH_FF_NAMESPACE_XATTRS, xattr, &INT32);
}

struct rbh_filter *
rbh_filter_compare_ns_xattr_int64_new(enum rbh_filter_operator op,
                                      const char *xattr, int64_t int64)
{
    const struct rbh_value INT64 = {
        .type = RBH_VT_INT64,
        .int64 = int64,
    };

    return rbh_filter_compare_new(op, RBH_FF_NAMESPACE_XATTRS, xattr, &INT64);
}

struct rbh_filter *
rbh_filter_compare_ns_xattr_string_new(enum rbh_filter_operator op,
                                       const char *xattr, const char *string)
{
    const struct rbh_value STRING = {
        .type = RBH_VT_STRING,
        .string = string,
    };

    return rbh_filter_compare_new(op, RBH_FF_NAMESPACE_XATTRS, xattr, &STRING);
}

struct rbh_filter *
rbh_filter_compare_ns_xattr_regex_new(enum rbh_filter_operator op,
                                      const char *xattr, const char *regex,
                                      unsigned int regex_options)
{
    const struct rbh_value REGEX = {
        .type = RBH_VT_REGEX,
        .regex = {
            .string = regex,
            .options = regex_options,
        },
    };

    return rbh_filter_compare_new(op, RBH_FF_NAMESPACE_XATTRS, xattr, &REGEX);
}

struct rbh_filter *
rbh_filter_compare_ns_xattr_sequence_new(enum rbh_filter_operator op,
                                         const char *xattr,
                                         const struct rbh_value values[],
                                         size_t count)
{
    const struct rbh_value SEQUENCE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = values,
            .count = count,
        },
    };

    return rbh_filter_compare_new(op, RBH_FF_NAMESPACE_XATTRS, xattr,
                                  &SEQUENCE);
}

struct rbh_filter *
rbh_filter_compare_ns_xattr_map_new(enum rbh_filter_operator op,
                                    const char *xattr,
                                    const struct rbh_value_pair pairs[],
                                    size_t count)
{
    const struct rbh_value MAP = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = pairs,
            .count = count,
        },
    };

    return rbh_filter_compare_new(op, RBH_FF_NAMESPACE_XATTRS, xattr, &MAP);
}

static struct rbh_filter *
filter_logical_new(enum rbh_filter_operator op,
                   const struct rbh_filter * const *filters, size_t count)
{
    const struct rbh_filter LOGICAL = {
        .op = op,
        .logical = {
            .filters = filters,
            .count = count,
        },
    };

    if (count == 0) {
        errno = EINVAL;
        return NULL;
    }

    return filter_clone(&LOGICAL);
}

struct rbh_filter *
rbh_filter_and_new(const struct rbh_filter * const *filters, size_t count)
{
    return filter_logical_new(RBH_FOP_AND, filters, count);
}

struct rbh_filter *
rbh_filter_or_new(const struct rbh_filter * const *filters, size_t count)
{
    return filter_logical_new(RBH_FOP_OR, filters, count);
}

struct rbh_filter *
rbh_filter_not_new(const struct rbh_filter *filter)
{
    return filter_logical_new(RBH_FOP_NOT, &filter, 1);
}

static int
comparison_filter_validate(const struct rbh_filter *filter)
{
    if (!op_matches_value(filter->op, &filter->compare.value))
        goto out_einval;

    switch (filter->compare.field) {
    case RBH_FF_ID:
    case RBH_FF_PARENT_ID:
    case RBH_FF_ATIME:
    case RBH_FF_MTIME:
    case RBH_FF_CTIME:
    case RBH_FF_NAME:
    case RBH_FF_TYPE:
    case RBH_FF_NAMESPACE_XATTRS:
    case RBH_FF_INODE_XATTRS:
        return rbh_value_validate(&filter->compare.value);
    }

out_einval:
    errno = EINVAL;
    return -1;
}

static int
logical_filter_validate(const struct rbh_filter *filter)
{
    if (filter->logical.count == 0) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < filter->logical.count; i++) {
        if (rbh_filter_validate(filter->logical.filters[i]))
            return -1;
    }

    return 0;
}

int
rbh_filter_validate(const struct rbh_filter *filter)
{
    if (filter == NULL)
        return 0;

    switch (filter->op) {
    case RBH_FOP_COMPARISON_MIN ... RBH_FOP_COMPARISON_MAX:
        return comparison_filter_validate(filter);
    case RBH_FOP_LOGICAL_MIN ... RBH_FOP_LOGICAL_MAX:
        return logical_filter_validate(filter);
    }

    errno = EINVAL;
    return -1;
}
