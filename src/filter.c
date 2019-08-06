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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "robinhood/filter.h"

struct rbh_filter *
rbh_filter_compare_binary_new(enum rbh_filter_operator op,
                              enum rbh_filter_field field, size_t size,
                              const char *data_)
{
    struct rbh_filter *filter;
    void *data;

    if (!rbh_is_comparison_operator(op) || op == RBH_FOP_REGEX
            || op == RBH_FOP_IN) {
        errno = EINVAL;
        return NULL;
    }

    filter = malloc(sizeof(*filter) + size);
    if (filter == NULL)
        return NULL;
    data = (char *)filter + sizeof(*filter);

    memcpy(data, data_, size);

    filter->op = op;
    filter->compare.field = field;
    filter->compare.value.type = RBH_FVT_BINARY;
    filter->compare.value.binary.size = size;
    filter->compare.value.binary.data = data;

    return filter;
}

struct rbh_filter *
rbh_filter_compare_int32_new(enum rbh_filter_operator op,
                             enum rbh_filter_field field, int32_t int32)
{
    struct rbh_filter *filter;

    if (!rbh_is_comparison_operator(op) || op == RBH_FOP_REGEX
            || op == RBH_FOP_IN) {
        errno = EINVAL;
        return NULL;
    }

    filter = malloc(sizeof(*filter));
    if (filter == NULL)
        return NULL;

    filter->op = op;
    filter->compare.field = field;
    filter->compare.value.type = RBH_FVT_INT32;
    filter->compare.value.int32 = int32;

    return filter;
}

struct rbh_filter *
rbh_filter_compare_int64_new(enum rbh_filter_operator op,
                             enum rbh_filter_field field, int64_t int64)
{
    struct rbh_filter *filter;

    if (!rbh_is_comparison_operator(op) || op == RBH_FOP_REGEX
            || op == RBH_FOP_IN) {
        errno = EINVAL;
        return NULL;
    }

    filter = malloc(sizeof(*filter));
    if (filter == NULL)
        return NULL;

    filter->op = op;
    filter->compare.field = field;
    filter->compare.value.type = RBH_FVT_INT64;
    filter->compare.value.int64 = int64;

    return filter;
}

struct rbh_filter *
rbh_filter_compare_string_new(enum rbh_filter_operator op,
                              enum rbh_filter_field field, const char *string)
{
    struct rbh_filter *filter;
    size_t length;
    void *data;

    if (!rbh_is_comparison_operator(op) || op == RBH_FOP_REGEX
            || op == RBH_FOP_IN) {
        errno = EINVAL;
        return NULL;
    }

    length = strlen(string) + 1;

    filter = malloc(sizeof(*filter) + length);
    if (filter == NULL)
        return NULL;
    data = (char *)filter + sizeof(*filter);

    memcpy(data, string, length);

    filter->op = op;
    filter->compare.field = field;
    filter->compare.value.type = RBH_FVT_STRING;
    filter->compare.value.string = data;

    return filter;
}

struct rbh_filter *
rbh_filter_compare_regex_new(enum rbh_filter_field field, const char *regex,
                             unsigned int regex_options)
{
    struct rbh_filter *filter;
    size_t length;
    void *data;

    length = strlen(regex) + 1;

    filter = malloc(sizeof(*filter) + length);
    if (filter == NULL)
        return NULL;
    data = (char *)filter + sizeof(*filter);

    memcpy(data, regex, length);

    filter->op = RBH_FOP_REGEX;
    filter->compare.field = field;
    filter->compare.value.type = RBH_FVT_REGEX;
    filter->compare.value.regex.string = data;
    filter->compare.value.regex.options = regex_options;

    return filter;
}

struct rbh_filter *
rbh_filter_compare_time_new(enum rbh_filter_operator op,
                            enum rbh_filter_field field, time_t time)
{
    struct rbh_filter *filter;

    if (!rbh_is_comparison_operator(op) || op == RBH_FOP_REGEX
            || op == RBH_FOP_IN) {
        errno = EINVAL;
        return NULL;
    }

    filter = malloc(sizeof(*filter));
    if (filter == NULL)
        return NULL;

    filter->op = op;
    filter->compare.field = field;
    filter->compare.value.type = RBH_FVT_TIME;
    filter->compare.value.time = time;

    return filter;
}

static ssize_t
filter_value_data_size(const struct rbh_filter_value *value)
{
    ssize_t data_size;

    switch (value->type) {
    case RBH_FVT_BINARY:
        return value->binary.size;
    case RBH_FVT_INT32:
    case RBH_FVT_INT64:
        return 0;
    case RBH_FVT_STRING:
        return strlen(value->string) + 1;
    case RBH_FVT_REGEX:
        return strlen(value->regex.string) + 1;
    case RBH_FVT_TIME:
        return 0;
    case RBH_FVT_LIST:
        data_size = value->list.count * sizeof(*value->list.elements);
        for (size_t i = 0; i < value->list.count; i++) {
            ssize_t tmp;

            tmp = filter_value_data_size(&value->list.elements[i]);
            if (tmp < 0)
                return tmp;

            data_size += tmp;
        }
        return data_size;
    }

    errno = EINVAL;
    return -1;
}

static void *
filter_value_copy(struct rbh_filter_value *dest,
                  const struct rbh_filter_value *src, void *data)
{
    dest->type = src->type;

    switch (src->type) {
    case RBH_FVT_BINARY:
        dest->binary.size = src->binary.size;
        memcpy(data, src->binary.data, src->binary.size);
        dest->binary.data = data;
        return (char *)data + src->binary.size;
    case RBH_FVT_INT32:
        dest->int32 = src->int32;
        return data;
    case RBH_FVT_INT64:
        dest->int64 = src->int64;
        return data;
    case RBH_FVT_STRING:
        strcpy(data, src->string);
        dest->string = data;
        return (char *)data + strlen(src->string) + 1;
    case RBH_FVT_REGEX:
        dest->regex.options = src->regex.options;
        strcpy(data, src->regex.string);
        dest->regex.string = data;
        return (char *)data + strlen(src->regex.string) + 1;
    case RBH_FVT_TIME:
        dest->time = src->time;
        return data;
    case RBH_FVT_LIST:
        dest->list.count = src->list.count;
        memcpy(data, src->list.elements,
               src->list.count * sizeof(*src->list.elements));
        dest->list.elements = data;
        data = (char *)data + src->list.count * sizeof(*src->list.elements);
        for (size_t i = 0; i < src->list.count; i++) {
            struct rbh_filter_value *element;

            /* Casting the const away is valid here because
             * `dest->list.elements' is set to point at dynamically allocated
             * memory (the initial value of `data') */
            element = (struct rbh_filter_value *)&dest->list.elements[i];
            data = filter_value_copy(element, &src->list.elements[i], data);
            if (data == NULL)
                return NULL;
        }
        return data;
    }

    errno = EINVAL;
    return NULL;
}

struct rbh_filter *
rbh_filter_compare_list_new(enum rbh_filter_field field, size_t count,
                            const struct rbh_filter_value values[])
{
    const struct rbh_filter_value list = {
        .type = RBH_FVT_LIST,
        .list = {
            .count = count,
            .elements = values,
        },
    };
    struct rbh_filter *filter;
    ssize_t data_size;
    void *data;

    data_size = filter_value_data_size(&list);
    if (data_size < 0)
        return NULL;

    filter = malloc(sizeof(*filter) + data_size);
    if (filter == NULL)
        return NULL;
    data = (char *)filter + sizeof(*filter);

    filter->op = RBH_FOP_IN;
    filter->compare.field = field;
    if (filter_value_copy(&filter->compare.value, &list, data) == NULL) {
        int save_errno = errno;

        free(filter);
        errno = save_errno;
        return NULL;
    }

    return filter;
}

static struct rbh_filter *
filter_compose_new(enum rbh_filter_operator op,
                   const struct rbh_filter * const filters[], size_t count)
{
    struct rbh_filter *filter;
    void *data;

    filter = malloc(sizeof(*filter) + count * sizeof(*filters));
    if (filter == NULL)
        return NULL;
    data = (char *)filter + sizeof(*filter);

    memcpy(data, filters, count * sizeof(*filters));

    filter->op = op;
    filter->logical.count = count;
    filter->logical.filters = data;

    return filter;
}

struct rbh_filter *
rbh_filter_and_new(const struct rbh_filter * const filters[], size_t count)
{
    return filter_compose_new(RBH_FOP_AND, filters, count);
}

struct rbh_filter *
rbh_filter_or_new(const struct rbh_filter * const filters[], size_t count)
{
    return filter_compose_new(RBH_FOP_OR, filters, count);
}

struct rbh_filter *
rbh_filter_not_new(const struct rbh_filter *filter)
{
    return filter_compose_new(RBH_FOP_NOT, &filter, 1);
}

void
rbh_filter_free(struct rbh_filter *filter)
{
    if (filter == NULL)
        return;

    if (rbh_is_logical_operator(filter->op)) {
        struct rbh_filter **filters;

        /* Casting the const away is assumed to be valid, it is the caller's
         * reponsability to make sure of it.
         */
        filters = (struct rbh_filter **)filter->logical.filters;

        for (size_t i = 0; i < filter->logical.count; i++)
            rbh_filter_free(filters[i]);
    }
    free(filter);
}

static int
logical_filter_validate(const struct rbh_filter *filter)
{
    if (filter->logical.count == 0) {
        errno = EINVAL;
        return -1;
    }

    if (filter->op == RBH_FOP_NOT && filter->logical.count != 1) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < filter->logical.count; i++) {
        if (rbh_filter_validate(filter->logical.filters[i]))
            return -1;
    }

    return 0;
}

#define FRO_MASK (RBH_FRO_MAX + (RBH_FRO_MAX - 1))

static int
comparison_filter_validate(const struct rbh_filter *filter)
{
    switch (filter->compare.value.type) {
    case RBH_FVT_BINARY:
    case RBH_FVT_INT32:
    case RBH_FVT_INT64:
    case RBH_FVT_STRING:
    case RBH_FVT_TIME:
        switch (filter->op) {
        case RBH_FOP_REGEX:
        case RBH_FOP_IN:
            errno = EINVAL;
            return -1;
        default:
            return 0;
        }
    case RBH_FVT_REGEX:
        /* TODO: maybe check the regex is a valide PCRE? */
        if (filter->op != RBH_FOP_REGEX) {
            errno = EINVAL;
            return -1;
        }
        if (filter->compare.value.regex.options & ~FRO_MASK) {
            errno = EINVAL;
            return -1;
        }
        return 0;
    case RBH_FVT_LIST:
        switch (filter->op) {
        case RBH_FOP_IN:
            return 0;
        case RBH_FOP_EQUAL:
        case RBH_FOP_LOWER_THAN:
        case RBH_FOP_LOWER_OR_EQUAL:
        case RBH_FOP_GREATER_THAN:
        case RBH_FOP_GREATER_OR_EQUAL:
            errno = ENOTSUP;
            return -1;
        default:
            errno = EINVAL;
            return -1;
        }
    }

    errno = EINVAL;
    return -1;
}

int
rbh_filter_validate(const struct rbh_filter *filter)
{
    if (filter == NULL)
        return 0;

    if (rbh_is_comparison_operator(filter->op))
        return comparison_filter_validate(filter);
    return logical_filter_validate(filter);
}

static struct rbh_filter *
logical_filter_clone(const struct rbh_filter *filter)
{
    struct rbh_filter **filters;
    struct rbh_filter *clone;
    int save_errno = errno;

    clone = malloc(sizeof(*clone) + filter->logical.count * sizeof(*filters));
    if (clone == NULL)
        return NULL;
    filters = (struct rbh_filter **)((char *)clone + sizeof(*clone));

    clone->op = filter->op;
    for (size_t i = 0; i < filter->logical.count; i++) {
        errno = 0;
        filters[i] = rbh_filter_clone(filter->logical.filters[i]);
        if (filters[i] == NULL && errno != 0) {
            save_errno = errno;
            for (size_t j = 0; j < i; j++)
                rbh_filter_free(filters[j]);
            free(clone);
            errno = save_errno;
            return NULL;
        }
    }
    clone->logical.filters = (const struct rbh_filter **)filters;
    clone->logical.count = filter->logical.count;

    /* If anyone ever wants to, some combinations of filters may be mergeable.
     * In which case, it should be done here.
     */

    errno = save_errno;
    return clone;
}

static struct rbh_filter *
comparison_filter_clone(const struct rbh_filter *filter)
{
    switch (filter->compare.value.type) {
    case RBH_FVT_BINARY:
        return rbh_filter_compare_binary_new(filter->op, filter->compare.field,
                                             filter->compare.value.binary.size,
                                             filter->compare.value.binary.data);
    case RBH_FVT_INT32:
        return rbh_filter_compare_int32_new(filter->op, filter->compare.field,
                                            filter->compare.value.int32);
    case RBH_FVT_INT64:
        return rbh_filter_compare_int64_new(filter->op, filter->compare.field,
                                            filter->compare.value.int64);
    case RBH_FVT_STRING:
        return rbh_filter_compare_string_new(filter->op, filter->compare.field,
                                             filter->compare.value.string);
    case RBH_FVT_REGEX:
        return rbh_filter_compare_regex_new(
                filter->compare.field, filter->compare.value.regex.string,
                filter->compare.value.regex.options
                );
    case RBH_FVT_TIME:
        return rbh_filter_compare_time_new(filter->op, filter->compare.field,
                                           filter->compare.value.time);
    case RBH_FVT_LIST:
        return rbh_filter_compare_list_new(filter->compare.field,
                                           filter->compare.value.list.count,
                                           filter->compare.value.list.elements);
    }

    errno = EINVAL;
    return NULL;
}

struct rbh_filter *
rbh_filter_clone(const struct rbh_filter *filter)
{
    if (filter == NULL)
        return NULL;

    if (rbh_is_comparison_operator(filter->op))
        return comparison_filter_clone(filter);
    return logical_filter_clone(filter);
}
