/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/xattr.h>

#include "selinux_internals.h"
#include "robinhood/statx.h"
#include "robinhood/utils.h"
#include "robinhood/backends/common.h"
#include "robinhood/backends/selinux.h"
#include "sstack.h"
#include "value.h"

#define SELINUX_XATTR_NAME    "security.selinux"

#define SELINUX_CONTEXT_XATTR "selinux.context"
#define SELINUX_USER_XATTR    "selinux.user"
#define SELINUX_ROLE_XATTR    "selinux.role"
#define SELINUX_TYPE_XATTR    "selinux.type"
#define SELINUX_RANGE_XATTR   "selinux.range"

#define SELINUX_LOW_SENS_XATTR  "selinux.low_sens"
#define SELINUX_HIGH_SENS_XATTR "selinux.high_sens"
#define SELINUX_LOW_CAT_XATTR   "selinux.low_cat"
#define SELINUX_HIGH_CAT_XATTR  "selinux.high_cat"

#define SELINUX_INTERVAL_FIRST_KEY "f"
#define SELINUX_INTERVAL_LAST_KEY  "l"

#define SELINUX_MAX_CATEGORY     1023
#define SELINUX_MAX_INTERVALS    (SELINUX_MAX_CATEGORY + 1)


/**
 * An interval of SELinux categories.
 *
 * A single category such as c4 is stored as [4, 4],
 * while c7.c9 is stored as [7, 9].
 */
struct selinux_interval {
    int32_t first;
    int32_t last;
};

/*
 * A SELinux level is composed of one sensitivity and a list of
 * category intervals
 *
 * Intervals are kept ordered and adjacent intervals are merged.
 * For instance, c1,c2,c4.c6 is stored as [1, 2], [4, 6].
 */
struct selinux_level{
    int32_t sens;
    struct selinux_interval intervals[SELINUX_MAX_INTERVALS];
    size_t intervals_count;
};

/*
 * A SELinux MLS/MCS range composed of a low and a high level.
 *
 * A simple level without a '-' separator is stored with identical
 * low and high levels.
 */
struct selinux_range{
    struct selinux_level low;
    struct selinux_level high;
};


static char*
value_to_string(const struct rbh_value *value)
{
    char *copy;

    if (value == NULL){
        errno = EINVAL;
        return NULL;
    }

    switch(value->type){
        case RBH_VT_STRING:
            return xstrdup(value->string);
        case RBH_VT_BINARY:
            copy = xmalloc(value->binary.size + 1);
            memcpy(copy, value->binary.data, value->binary.size);
            copy[value->binary.size] = '\0';
            return copy;
        default:
            errno = EINVAL;
            return NULL;
    }
}


static char*
get_selinux_context(struct entry_info *einfo)
{
    ssize_t count;

    if (einfo == NULL || einfo->inode_xattrs == NULL ||
        einfo->inode_xattrs_count == NULL) {
        return NULL;
    }

    count = *einfo->inode_xattrs_count;

    for (ssize_t i = 0; i < count; i++) {
        const struct rbh_value_pair *pair = &einfo->inode_xattrs[i];

        if (strcmp(pair->key, SELINUX_XATTR_NAME) != 0)
            continue;

        return value_to_string(pair->value);
    }

    return NULL;
}


static int
parse_number(const char **cursor)
{
    const char *p = *cursor;
    int value = 0;

    if (*p < '0' || *p > '9') {
        errno = EINVAL;
        return -1;
    }

    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        p++;
    }

    *cursor = p;
    return value;
}

/* Parse a SELinux sensitivity in the form s<number>. */
static int
parse_sensitivity(const char **cursor)
{
    const char *p = *cursor;
    int sens;

    if (*p != 's') {
        errno = EINVAL;
        return -1;
    }

    p++;

    sens = parse_number(&p);
    if (sens < 0)
        return -1;

    *cursor = p;
    return sens;
}

/* Parse a SELinux category in the form c<number>. */
static int
parse_category(const char **cursor)
{
    const char *p = *cursor;
    int cat;

    if (*p != 'c') {
        errno = EINVAL;
        return -1;
    }

    p++;

    cat = parse_number(&p);
    if (cat < 0 || cat > SELINUX_MAX_CATEGORY) {
        errno = EINVAL;
        return -1;
    }

    *cursor = p;
    return cat;
}


static int
append_interval(struct selinux_level *level, int32_t first, int32_t last)
{
    struct selinux_interval *previous;

    if (last < first) {
        errno = EINVAL;
        return -1;
    }

    if (level->intervals_count > 0) {
        previous = &level->intervals[level->intervals_count - 1];
        /*
         * Merge the new interval with the previous one when they are adjacent.
         */
        if (first >= previous->first && first <= previous->last + 1) {
            if (last > previous->last)
                previous->last = last;

            return 0;
        }

        if (first <= previous->last) {
            errno = EINVAL;
            return -1;
        }
    }

    level->intervals[level->intervals_count].first = first;
    level->intervals[level->intervals_count].last = last;
    level->intervals_count++;

    return 0;
}

/**
 * Parse either one category or one category interval.
 */
static int
parse_category_item(const char **cursor, struct selinux_level *level)
{
    const char *p = *cursor;
    int first;
    int last;

    first = parse_category(&p);
    if (first < 0)
        return -1;

    last = first;

    if (*p == '.') {
        p++;

        last = parse_category(&p);
        if (last < 0)
            return -1;
    }

    if (append_interval(level, first, last))
        return -1;

    *cursor = p;
    return 0;
}


/**
 * Parse a category separated by a comma and append its intervals to the level.
 */
static int
parse_category_set(const char **cursor, struct selinux_level *level)
{
    const char *p = *cursor;

    if (*p == '\0'){
        *cursor = p;
        return 0;
    }

    for (;;) {
        if (parse_category_item(&p, level))
            return -1;

        if (*p == '\0')
            break;

        if (*p != ',') {
            errno = EINVAL;
            return -1;
        }

        p++;

        if (*p == '\0') {
            errno = EINVAL;
            return -1;
        }
    }

    *cursor = p;
    return 0;
}


/**
 * Parse one SELinux level in the form:
 *      sensitivity[:category_set].
 */
static int
parse_level(const char *string, struct selinux_level *level)
{
    const char *p = string;

    memset(level, 0, sizeof(*level));

    level->sens = parse_sensitivity(&p);
    if (level->sens < 0)
        return -1;

    if (*p == ':') {

        p++;

        if (parse_category_set(&p, level))
            return -1;
    }

    if (*p != '\0') {
        errno = EINVAL;
        return -1;
    }

    return 0;
}


/**
 * Parse a simple SELinux level or a low-high range.
 */
static int
parse_range(char *range, struct selinux_range *parsed)
{
    char *dash;

    memset(parsed, 0, sizeof(*parsed));

    dash = strchr(range, '-');
    if (dash != NULL) {
        *dash = '\0';

        if (parse_level(range, &parsed->low))
            return -1;

        if (parse_level(dash + 1, &parsed->high))
            return -1;
    } else {
        if (parse_level(range, &parsed->low))
            return -1;
        parsed->high = parsed->low;
    }
    return 0;
}


/**
 * Add a pair whose value is a sequence of category interval documents.
 *
 * For example, the following intervals:
 *      [1, 1], [4, 4], [7, 9]
 *
 * are stored into the backend as:
 *      [ { f: 1, l: 1 }, { f: 4, l: 4 }, { f: 7, l: 9 } ]
 */
static int
add_interval_list_pair(const char *key,
                       const struct selinux_interval *intervals,
                       size_t intervals_count,
                       struct rbh_value_pair *pair,
                       struct rbh_sstack *values)
{
    struct rbh_value *seq;
    struct rbh_value *items;

    pair->key = key;

    seq = rbh_sstack_push(values, NULL, sizeof(*seq));
    if (seq == NULL)
        return -1;


    pair->value = seq;

    seq->type = RBH_VT_SEQUENCE;
    seq->sequence.count = intervals_count;
    seq->sequence.values = NULL;

    if (intervals_count == 0)
        return 0;

    items = rbh_sstack_push(values, NULL, intervals_count * sizeof(*items));

    seq->sequence.values = items;

    for (int i = 0; i < intervals_count; i++) {
        struct rbh_value *item = &items[i];
        struct rbh_value_pair *fields;
        struct rbh_value *first;
        struct rbh_value *last;

        fields = rbh_sstack_push(values, NULL, 2 * sizeof(*fields));

        first = rbh_sstack_push(values, NULL, sizeof(*first));

        last = rbh_sstack_push(values, NULL, sizeof(*last));

        first->type = RBH_VT_INT32;
        first->int32 = intervals[i].first;

        last->type = RBH_VT_INT32;
        last->int32 = intervals[i].last;

        fields[0].key = SELINUX_INTERVAL_FIRST_KEY;
        fields[0].value = first;

        fields[1].key = SELINUX_INTERVAL_LAST_KEY;
        fields[1].value = last;

        item->type = RBH_VT_MAP;
        item->map.count = 2;
        item->map.pairs = fields;
    }
    return 0;
}


static int
fill_selinux_pairs(char *ctx, struct rbh_value_pair *pairs,
                   struct rbh_sstack *values)
{
    struct selinux_range parsed_range;
    char *range_copy;
    char *range;
    char *sep1;
    char *sep2;
    char *sep3;
    int count = 0;

    sep1 = strchr(ctx, ':');
    if (sep1 == NULL)
        goto invalid;

    sep2 = strchr(sep1 + 1, ':');
    if (sep2 == NULL)
        goto invalid;

    sep3 = strchr(sep2 + 1, ':');
    if (sep3 == NULL)
        goto invalid;

    if (sep1 == ctx || sep2 == sep1 + 1 ||
        sep3 == sep2 + 1 || sep3[1] == '\0')
        goto invalid;

    range = sep3 + 1;

    if (fill_string_pair(SELINUX_CONTEXT_XATTR, ctx,
                         &pairs[count++], values))
        return -1;

    *sep1 = '\0';
    *sep2 = '\0';
    *sep3 = '\0';

    if (fill_string_pair(SELINUX_USER_XATTR, ctx,
                         &pairs[count++], values))
        return -1;

    if (fill_string_pair(SELINUX_ROLE_XATTR, sep1 + 1,
                         &pairs[count++], values))
        return -1;

    if (fill_string_pair(SELINUX_TYPE_XATTR, sep2 + 1,
                         &pairs[count++], values))
        return -1;

    if (fill_string_pair(SELINUX_RANGE_XATTR, range,
                         &pairs[count++], values))
        return -1;

    range_copy = xstrdup(range);

    if (parse_range(range_copy, &parsed_range)) {
        free(range_copy);
        return count;
    }

    free(range_copy);

    if (fill_int32_pair(SELINUX_LOW_SENS_XATTR,
                        parsed_range.low.sens,
                        &pairs[count++], values))
        return -1;

    if (fill_int32_pair(SELINUX_HIGH_SENS_XATTR,
                        parsed_range.high.sens,
                        &pairs[count++], values))
        return -1;

    if (add_interval_list_pair(SELINUX_LOW_CAT_XATTR,
                               parsed_range.low.intervals,
                               parsed_range.low.intervals_count,
                               &pairs[count++], values))
        return -1;

    if (add_interval_list_pair(SELINUX_HIGH_CAT_XATTR,
                               parsed_range.high.intervals,
                               parsed_range.high.intervals_count,
                               &pairs[count++], values))
        return -1;

    return count;

invalid:
    errno = EINVAL;
    return -1;
}


int
rbh_selinux_enrich(struct entry_info *einfo, uint64_t flags,
                  struct rbh_value_pair *pairs, size_t pairs_count,
          	  struct rbh_sstack *values)
{
    char* context;
    int count;

    if (!rbh_attr_is_selinux(flags))
        return 0;

    if (pairs_count < 9) {
        errno = ENOMEM;
        return -1;
    }

    context = get_selinux_context(einfo);
    if (context == NULL)
        return 0;

    count = fill_selinux_pairs(context, pairs, values);

    free(context);
    return count;
}


int
rbh_selinux_setup(void)
{
    return 0;
}
