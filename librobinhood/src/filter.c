/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <sys/stat.h>

#include "robinhood/filter.h"
#include "robinhood/statx.h"
#include "robinhood/sstack.h"
#include "robinhood/utils.h"

#include "utils.h"
#include "value.h"

static ssize_t
filter_field_data_size(const struct rbh_filter_field *field)
{
    switch (field->fsentry) {
    case RBH_FP_ID:
    case RBH_FP_PARENT_ID:
    case RBH_FP_NAME:
    case RBH_FP_STATX:
    case RBH_FP_SYMLINK:
        return 0;
    case RBH_FP_NAMESPACE_XATTRS:
    case RBH_FP_INODE_XATTRS:
        return field->xattr ? strlen(field->xattr) + 1 : 0;
    default:
        errno = EINVAL;
        return -1;
    }
}

static int
filter_field_copy(struct rbh_filter_field *dest,
                  const struct rbh_filter_field *src, char **buffer,
                  size_t *bufsize)
{
    size_t size = *bufsize;
    char *data = *buffer;

    /* dest->fsentry */
    dest->fsentry = src->fsentry;

    switch (src->fsentry) {
    case RBH_FP_ID:
    case RBH_FP_PARENT_ID:
    case RBH_FP_NAME:
    case RBH_FP_SYMLINK:
        break;
    case RBH_FP_STATX:
        /* dest->statx */
        dest->statx = src->statx;
        break;
    case RBH_FP_NAMESPACE_XATTRS:
    case RBH_FP_INODE_XATTRS:
        /* dest->xattr */
        if (src->xattr == NULL) {
            dest->xattr = NULL;
            break;
        }

        assert(size >= strlen(src->xattr) + 1);
        dest->xattr = data;
        data = mempcpy(data, src->xattr, strlen(src->xattr) + 1);
        size -= strlen(src->xattr) + 1;
        break;
    }

    *buffer = data;
    *bufsize = size;
    return 0;
}

static int
comparison_filter_copy(struct rbh_filter *dest, const struct rbh_filter *src,
                       char **buffer, size_t *bufsize)
{
    size_t size = *bufsize;
    char *data = *buffer;

    /* dest->compare.field */
    if (filter_field_copy(&dest->compare.field, &src->compare.field, &data,
                          &size))
        return -1;

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
array_filter_copy(struct rbh_filter *dest, const struct rbh_filter *src,
                  char **buffer, size_t *bufsize)
{
    const struct rbh_filter **filters;
    size_t size = *bufsize;
    char *data = *buffer;

    /* dest->array.filters */
    filters = aligned_memalloc(alignof(*filters),
                               src->array.count * sizeof(*filters), &data,
                               &size);
    assert(filters);

    /* dest->array.field */
    if (filter_field_copy(&dest->array.field, &src->array.field, &data,
                          &size))
        return -1;

    for (size_t i = 0; i < src->array.count; i++) {
        struct rbh_filter *filter;

        if (src->array.filters[i] == NULL) {
            filters[i] = NULL;
            continue;
        }

        filter = aligned_memalloc(alignof(*filter), sizeof(*filter), &data,
                                  &size);
        assert(filter);

        if (filter_copy(filter, src->array.filters[i], &data, &size))
            return -1;

        filters[i] = filter;
    }
    dest->array.filters = filters;

    /* dest->array.count */
    dest->array.count = src->array.count;

    *buffer = data;
    *bufsize = size;
    return 0;
}

static int
get_filter_copy(struct rbh_filter *dest, const struct rbh_filter *src,
                char **buffer, size_t *bufsize)
{
    struct rbh_filter *fsentry;
    struct rbh_filter *filter;
    size_t size = *bufsize;
    char *data = *buffer;

    if (filter_field_copy(&dest->get.field, &src->get.field, &data, &size))
        return -1;

    fsentry = aligned_memalloc(alignof(*fsentry), sizeof(*fsentry), &data,
                               &size);
    assert(fsentry);
    if (filter_copy(fsentry, src->get.fsentry_to_get, &data, &size))
        return -1;
    dest->get.fsentry_to_get = fsentry;

    filter = aligned_memalloc(alignof(*filter), sizeof(*filter), &data,
                               &size);
    assert(filter);
    if (filter_copy(filter, src->get.filter, &data, &size))
        return -1;
    dest->get.filter = filter;

    *buffer = data;
    *bufsize = size;
    return 0;
}

static int
logical_filter_copy(struct rbh_filter *dest, const struct rbh_filter *src,
                    char **buffer, size_t *bufsize)
{
    const struct rbh_filter **filters;
    size_t size = *bufsize;
    char *data = *buffer;

    /* dest->logical.filters */
    filters = aligned_memalloc(alignof(*filters),
                               src->logical.count * sizeof(*filters), &data,
                               &size);
    assert(filters);

    for (size_t i = 0; i < src->logical.count; i++) {
        struct rbh_filter *filter;

        if (src->logical.filters[i] == NULL) {
            filters[i] = NULL;
            continue;
        }

        filter = aligned_memalloc(alignof(*filter), sizeof(*filter), &data,
                                  &size);
        assert(filter);

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
    else if (rbh_is_array_operator(src->op))
        return array_filter_copy(dest, src, buffer, bufsize);
    else if (rbh_is_get_operator(src->op))
        return get_filter_copy(dest, src, buffer, bufsize);
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
        size = filter_field_data_size(&filter->compare.field);
        return size + value_data_size(&filter->compare.value, size);
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
    case RBH_FOP_ARRAY_MIN ... RBH_FOP_ARRAY_MAX:
        size = filter_field_data_size(&filter->array.field);
        size += sizeof(filter) * filter->array.count;
        for (size_t i = 0; i < filter->array.count; i++) {
            if (filter->array.filters[i] == NULL)
                continue;

            size = sizealign(size, alignof(*filter));
            size += sizeof(*filter);
            if (filter_data_size(filter->array.filters[i]) < 0)
                return -1;
            size += filter_data_size(filter->array.filters[i]);
        }
        return size;
    case RBH_FOP_GET_MIN ... RBH_FOP_GET_MAX:
        size = filter_field_data_size(&filter->get.field);
        size += sizeof(filter) * 2;

        size = sizealign(size, alignof(*filter));
        size += sizeof(*filter);
        if (filter_data_size(filter->get.filter) < 0)
            return -1;
        size += filter_data_size(filter->get.filter);

        size = sizealign(size, alignof(*filter));
        size += sizeof(*filter);
        if (filter_data_size(filter->get.fsentry_to_get) < 0)
            return -1;
        size += filter_data_size(filter->get.fsentry_to_get);

        return size;
    }

    errno = EINVAL;
    return -1;
}

struct rbh_filter *
rbh_filter_clone(const struct rbh_filter *filter)
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
    case RBH_FOP_EXISTS:
        if (value->type != RBH_VT_BOOLEAN)
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
rbh_filter_compare_new(enum rbh_filter_operator op,
                       const struct rbh_filter_field *field,
                       const struct rbh_value *value)
{
    const struct rbh_filter COMPARE = {
        .op = op,
        .compare = {
            .field = *field,
            .value = *value,
        },
    };

    if (!op_matches_value(op, value)) {
        errno = EINVAL;
        return NULL;
    }

    return rbh_filter_clone(&COMPARE);
}

struct rbh_filter *
rbh_filter_compare_boolean_new(enum rbh_filter_operator op,
                               const struct rbh_filter_field *field,
                               bool boolean)
{
    const struct rbh_value boolean_ = {
        .type = RBH_VT_BOOLEAN,
        .boolean = boolean,
    };

    return rbh_filter_compare_new(op, field, &boolean_);
}

struct rbh_filter *
rbh_filter_compare_int32_new(enum rbh_filter_operator op,
                             const struct rbh_filter_field *field,
                             int32_t int32)
{
    const struct rbh_value int32_ = {
        .type = RBH_VT_INT32,
        .int32 = int32,
    };

    return rbh_filter_compare_new(op, field, &int32_);
}

struct rbh_filter *
rbh_filter_compare_uint32_new(enum rbh_filter_operator op,
                              const struct rbh_filter_field *field,
                              uint32_t uint32)
{
    const struct rbh_value uint32_ = {
        .type = RBH_VT_UINT32,
        .uint32 = uint32,
    };

    return rbh_filter_compare_new(op, field, &uint32_);
}

struct rbh_filter *
rbh_filter_compare_int64_new(enum rbh_filter_operator op,
                             const struct rbh_filter_field *field,
                             int64_t int64)
{
    const struct rbh_value int64_ = {
        .type = RBH_VT_INT64,
        .int64 = int64,
    };

    return rbh_filter_compare_new(op, field, &int64_);
}

struct rbh_filter *
rbh_filter_compare_uint64_new(enum rbh_filter_operator op,
                              const struct rbh_filter_field *field,
                              uint64_t uint64)
{
    const struct rbh_value uint64_ = {
        .type = RBH_VT_UINT64,
        .uint64 = uint64,
    };

    return rbh_filter_compare_new(op, field, &uint64_);
}

struct rbh_filter *
rbh_filter_compare_string_new(enum rbh_filter_operator op,
                              const struct rbh_filter_field *field,
                              const char *string)
{
    const struct rbh_value string_ = {
        .type = RBH_VT_STRING,
        .string = string,
    };

    return rbh_filter_compare_new(op, field, &string_);
}

struct rbh_filter *
rbh_filter_compare_binary_new(enum rbh_filter_operator op,
                              const struct rbh_filter_field *field,
                              const char *data, size_t size)
{
    const struct rbh_value binary = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = data,
            .size = size,
        },
    };

    return rbh_filter_compare_new(op, field, &binary);
}

struct rbh_filter *
rbh_filter_compare_regex_new(enum rbh_filter_operator op,
                             const struct rbh_filter_field *field,
                             const char *regex,
                             unsigned int regex_options)
{
    const struct rbh_value regex_ = {
        .type = RBH_VT_REGEX,
        .regex = {
            .string = regex,
            .options = regex_options,
        },
    };

    return rbh_filter_compare_new(op, field, &regex_);
}

struct rbh_filter *
rbh_filter_compare_sequence_new(enum rbh_filter_operator op,
                                const struct rbh_filter_field *field,
                                const struct rbh_value values[], size_t count)
{
    const struct rbh_value sequence = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = values,
            .count = count,
        },
    };

    return rbh_filter_compare_new(op, field, &sequence);
}

struct rbh_filter *
rbh_filter_compare_map_new(enum rbh_filter_operator op,
                           const struct rbh_filter_field *field,
                           const struct rbh_value_pair pairs[], size_t count)
{
    const struct rbh_value map = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = pairs,
            .count = count,
        },
    };

    return rbh_filter_compare_new(op, field, &map);
}

static struct rbh_filter *
filter_logical_new(enum rbh_filter_operator op,
                   const struct rbh_filter * const *filters, size_t count)
{
    const struct rbh_filter logical = {
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

    return rbh_filter_clone(&logical);
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

struct rbh_filter *
rbh_filter_exists_new(const struct rbh_filter_field *field)
{
    const struct rbh_value boolean_ = {
        .type = RBH_VT_BOOLEAN,
        .boolean = true,
    };

    return rbh_filter_compare_new(RBH_FOP_EXISTS, field, &boolean_);
}

struct rbh_filter *
rbh_filter_array_new(enum rbh_filter_operator op,
                     const struct rbh_filter_field *field,
                     const struct rbh_filter * const *filters, size_t count)
{
    const struct rbh_filter ARRAY = {
        .op = op,
        .array = {
            .field = *field,
            .filters = filters,
            .count = count
        },
    };

    if (count == 0) {
        errno = EINVAL;
        return NULL;
    }

    return rbh_filter_clone(&ARRAY);
}

struct rbh_filter *
rbh_filter_array_elemmatch_new(const struct rbh_filter_field *field,
                               const struct rbh_filter * const *filters,
                               size_t count)
{
    return rbh_filter_array_new(RBH_FOP_ELEMMATCH, field, filters, count);
}

struct rbh_filter *
rbh_filter_get_new(struct rbh_filter *filter,
                   const struct rbh_filter *fsentry_to_get,
                   const struct rbh_filter_field *field)
{
    const struct rbh_filter GET = {
        .op = RBH_FOP_GET,
        .get = {
            .filter = filter,
            .fsentry_to_get = fsentry_to_get,
            .field = *field,
        },
    };

    return rbh_filter_clone(&GET);
}

static struct rbh_sstack *filters;

static void __attribute__((constructor))
init_filters(void)
{
    const int MIN_FILTER_ALLOC = 32;

    filters = rbh_sstack_new(MIN_FILTER_ALLOC * sizeof(struct rbh_filter *));
    if (filters == NULL)
        error(EXIT_FAILURE, errno, "rbh_sstack_new");
}

static void __attribute__((destructor))
exit_filters(void)
{
    struct rbh_filter **filter;
    size_t size;

    while (true) {
        int rc;

        filter = rbh_sstack_peek(filters, &size);
        if (size == 0)
            break;

        assert(size % sizeof(*filter) == 0);

        for (size_t i = 0; i < size / sizeof(*filter); i++, filter++)
            free(*filter);

        rc = rbh_sstack_pop(filters, size);
        assert(rc == 0);
    }
    rbh_sstack_destroy(filters);
}

static struct rbh_filter *
filter_compose(enum rbh_filter_operator op, struct rbh_filter *left,
               struct rbh_filter *right)
{
    const struct rbh_filter **array;
    struct rbh_filter *filter;

    assert(op == RBH_FOP_AND || op == RBH_FOP_OR);

    filter = malloc(sizeof(*filter));
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    array = RBH_SSTACK_PUSH(filters, NULL, sizeof(*array) * 2);

    array[0] = left;
    array[1] = right;

    filter->op = op;
    filter->logical.filters = array;
    filter->logical.count = 2;

    return filter;
}

struct rbh_filter *
rbh_filter_and(struct rbh_filter *left, struct rbh_filter *right)
{
    return filter_compose(RBH_FOP_AND, left, right);
}

struct rbh_filter *
rbh_filter_or(struct rbh_filter *left, struct rbh_filter *right)
{
    return filter_compose(RBH_FOP_OR, left, right);
}

struct rbh_filter *
rbh_filter_array_compose(struct rbh_filter *left, struct rbh_filter *right)
{
    const struct rbh_filter **array;
    struct rbh_filter *filter;

    filter = malloc(sizeof(*filter));
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    array = RBH_SSTACK_PUSH(filters, NULL, sizeof(*array) * 2);

    array[0] = left;
    array[1] = right;

    filter->op = RBH_FOP_ELEMMATCH;
    filter->array.filters = array;
    filter->array.count = 2;

    return filter;
}

struct rbh_filter *
rbh_filter_not(struct rbh_filter *filter)
{
    struct rbh_filter *not;

    not = malloc(sizeof(*not));
    if (not == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    not->logical.filters = RBH_SSTACK_PUSH(filters, &filter, sizeof(filter));

    not->op = RBH_FOP_NOT;
    not->logical.count = 1;

    return not;
}

struct rbh_filter *
rbh_filetype2filter(const char *_filetype)
{
    struct rbh_filter_field field = {
        .fsentry = RBH_FP_STATX,
        .statx = RBH_STATX_TYPE
    };
    struct rbh_filter *filter;
    int filetype;

    if (_filetype[0] == '\0' || _filetype[1] != '\0')
        error(EX_USAGE, 0, "arguments to -type should only contain one letter");

    switch (_filetype[0]) {
    case 'b':
        filetype = S_IFBLK;
        break;
    case 'c':
        filetype = S_IFCHR;
        break;
    case 'd':
        filetype = S_IFDIR;
        break;
    case 'f':
        filetype = S_IFREG;
        break;
    case 'l':
        filetype = S_IFLNK;
        break;
    case 'p':
        filetype = S_IFIFO;
        break;
    case 's':
        filetype = S_IFSOCK;
        break;
    default:
        error(EX_USAGE, 0, "unknown argument to -type: %s", _filetype);
        /* clang: -Wsometimes-unitialized: `filtetype` */
        __builtin_unreachable();
    }

    filter = rbh_filter_compare_int32_new(RBH_FOP_EQUAL, &field, filetype);
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "filter_compare_integer");

    return filter;
}

void
rbh_get_size_parameters(const char *_size, char *operator, uint64_t *unit_size,
                        uint64_t *size)
{
    char op = *_size;
    char *suffix;

    switch (op) {
    case '+':
        *operator = '+';
        break;
    case '-':
        *operator = '-';
        break;
    default:
        *operator = 0;
    }

    if (*operator)
        _size++;

    errno = 0;
    *size = strtoull(_size, &suffix, 10);
    if (errno)
        error(EX_USAGE, EOVERFLOW, "invalid size argument `%s'", _size);

    if (*size == 0ULL && _size == suffix)
        error(EX_USAGE, 0,
              "size arguments should start with at least one digit");

    switch (*suffix++) {
    case 'T':
        *unit_size = 1099511627776;
        break;
    case 'G':
        *unit_size = 1073741824;
        break;
    case 'M':
        *unit_size = 1048576;
        break;
    case 'k':
        *unit_size = 1024;
        break;
    case '\0':
        /* default suffix */
        suffix--;
        __attribute__((fallthrough));
    case 'b':
        *unit_size = 512;
        break;
    case 'w':
        *unit_size = 2;
        break;
    case 'c':
        *unit_size = 1;
        break;
    default:
        error(EX_USAGE, 0, "invalid size argument `%s'", _size);
    }

    if (*suffix)
        error(EX_USAGE, 0, "invalid size argument `%s'", _size);
}

struct rbh_filter *
rbh_numeric2filter(const struct rbh_filter_field *field, const char *_numeric,
                   const enum rbh_filter_operator no_sign_op)
{
    const char *numeric = _numeric;
    struct rbh_filter *filter;
    char operator = *numeric;
    uint64_t value;
    int save_errno;

    switch (operator) {
    case '-':
    case '+':
        numeric++;
    }

    save_errno = errno;
    if (str2uint64_t(numeric, &value))
        return NULL;

    errno = save_errno;

    switch (operator) {
    case '-':
        filter = rbh_filter_compare_uint64_new(RBH_FOP_STRICTLY_LOWER, field,
                                               value);
        break;
    case '+':
        filter = rbh_filter_compare_uint64_new(RBH_FOP_STRICTLY_GREATER, field,
                                               value);
        break;
    default:
        filter = rbh_filter_compare_uint64_new(no_sign_op, field, value);
        break;
    }

    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_filter_compare_uint64_new");

    return filter;
}

struct rbh_filter *
rbh_shell_regex2filter(const struct rbh_filter_field *field,
                       const char *shell_regex, unsigned int regex_options)
{
    struct rbh_filter *filter;

    filter = rbh_filter_compare_regex_new(RBH_FOP_REGEX, field, shell_regex,
                                          regex_options);
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__ - 2,
                      "building a regex filter for %s", shell_regex);
    return filter;
}

static int
filter_field_validate(const struct rbh_filter_field *field)
{
    switch (field->fsentry) {
    case RBH_FP_ID:
    case RBH_FP_PARENT_ID:
    case RBH_FP_NAME:
    case RBH_FP_SYMLINK:
    case RBH_FP_NAMESPACE_XATTRS:
    case RBH_FP_INODE_XATTRS:
        return 0;
    case RBH_FP_STATX:
        switch (field->statx) {
        case RBH_STATX_TYPE:
        case RBH_STATX_MODE:
        case RBH_STATX_NLINK:
        case RBH_STATX_UID:
        case RBH_STATX_GID:
        case RBH_STATX_ATIME_SEC:
        case RBH_STATX_MTIME_SEC:
        case RBH_STATX_CTIME_SEC:
        case RBH_STATX_INO:
        case RBH_STATX_SIZE:
        case RBH_STATX_BLOCKS:
        case RBH_STATX_BTIME_SEC:
        case RBH_STATX_BLKSIZE:
        case RBH_STATX_ATTRIBUTES:
        case RBH_STATX_ATIME_NSEC:
        case RBH_STATX_BTIME_NSEC:
        case RBH_STATX_CTIME_NSEC:
        case RBH_STATX_MTIME_NSEC:
        case RBH_STATX_RDEV_MAJOR:
        case RBH_STATX_RDEV_MINOR:
        case RBH_STATX_DEV_MAJOR:
        case RBH_STATX_DEV_MINOR:
            return 0;
        }
        break;
    }

    errno = EINVAL;
    return -1;
}

static int
comparison_filter_validate(const struct rbh_filter *filter)
{
    if (!op_matches_value(filter->op, &filter->compare.value)) {
        errno = EINVAL;
        return -1;
    }

    if (filter_field_validate(&filter->compare.field))
        return -1;

    return rbh_value_validate(&filter->compare.value);
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

static int
sub_array_comparison_filter_validate(const struct rbh_filter *filter)
{
    if (filter->op < RBH_FOP_COMPARISON_MIN ||
        filter->op > RBH_FOP_COMPARISON_MAX) {
        errno = EINVAL;
        return -1;
    }

    if (!op_matches_value(filter->op, &filter->compare.value)) {
        errno = EINVAL;
        return -1;
    }

    return rbh_value_validate(&filter->compare.value);
}

static int
array_filter_validate(const struct rbh_filter *filter)
{
    if (filter->array.count == 0) {
        errno = EINVAL;
        return -1;
    }

    if (filter_field_validate(&filter->array.field))
        return -1;

    for (size_t i = 0; i < filter->array.count; i++) {
        if (sub_array_comparison_filter_validate(filter->array.filters[i]))
            return -1;
    }

    return 0;
}

static int
get_filter_validate(const struct rbh_filter *filter)
{
    if (filter_field_validate(&filter->get.field))
        return -1;

    return comparison_filter_validate(filter->get.filter) &&
           comparison_filter_validate(filter->get.fsentry_to_get);
}

int
rbh_filter_validate(const struct rbh_filter *filter)
{
    if (filter == NULL)
        return 0;

    switch (filter->op) {
    case RBH_FOP_COMPARISON_MIN ... RBH_FOP_COMPARISON_MAX:
        return comparison_filter_validate(filter);
    case RBH_FOP_NOT:
        if (filter->logical.count != 1) {
            errno = EINVAL;
            return -1;
        }
        __attribute__((fallthrough));
    case RBH_FOP_AND:
    case RBH_FOP_OR:
        return logical_filter_validate(filter);
    case RBH_FOP_ARRAY_MIN ... RBH_FOP_ARRAY_MAX:
        return array_filter_validate(filter);
    case RBH_FOP_GET_MIN ... RBH_FOP_GET_MAX:
        return get_filter_validate(filter);
    }

    errno = EINVAL;
    return -1;
}

const struct rbh_filter_field *
str2filter_field(const char *string_)
{
    static struct rbh_filter_field field;
    const char *string = string_;

    switch (*string++) {
    case 'i': /* id */
        if (strcmp(string, "d"))
            break;
        field.fsentry = RBH_FP_ID;
        return &field;
    case 'n': /* name, ns-xattrs */
        switch (*string++) {
        case 'a': /* name */
            if (strcmp(string, "me"))
                break;
            field.fsentry = RBH_FP_NAME;
            return &field;
        case 's': /* ns-xattrs */
            if (strncmp(string, "-xattrs", 7))
                break;
            field.fsentry = RBH_FP_NAMESPACE_XATTRS;
            string += 6;

            switch (*string++) {
            case '\0':
                field.xattr = NULL;
                return &field;
            case '.':
                field.xattr = string;
                return &field;
            }
            break;
        }
        break;
    case 'p': /* parent-id */
        if (strcmp(string, "arent-id"))
            break;
        field.fsentry = RBH_FP_PARENT_ID;
        return &field;
    case 's': /* statx, symlink */
        switch (*string++) {
        case 't':
            if (strncmp(string, "atx", 3))
                break;
            string += 3;

            field.fsentry = RBH_FP_STATX;
            switch (*string++) {
            case '\0':
                field.statx = RBH_STATX_ALL;
                return &field;
            case '.':
                field.statx = str2statx(string);
                if (field.statx == 0)
                    return NULL;

                return &field;
            }
            break;
        case 'y':
            if (strcmp(string, "mlink"))
                break;
            field.fsentry = RBH_FP_SYMLINK;
            return &field;
        }
        break;
    case 'x': /* xattrs */
        if (strncmp(string, "attrs", 5))
            break;
        string += 5;

        field.fsentry = RBH_FP_INODE_XATTRS;
        switch (*string++) {
        case '\0':
            field.xattr = NULL;
            return &field;
        case '.':
            field.xattr = string;
            return &field;
        }
    }

    error(EX_USAGE, 0, "unexpected field string: '%s'", string_);
    __builtin_unreachable();
}
