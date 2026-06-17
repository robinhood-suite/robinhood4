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
#include "range_parser.h"

#define SELINUX_XATTR_NAME    "security.selinux"

#define SELINUX_CONTEXT_XATTR "selinux.context"
#define SELINUX_USER_XATTR    "selinux.user"
#define SELINUX_ROLE_XATTR    "selinux.role"
#define SELINUX_TYPE_XATTR    "selinux.type"
#define SELINUX_RANGE_XATTR   "selinux.range"

#define SELINUX_HIGH_SENS_XATTR "selinux.high_sens"
#define SELINUX_LOW_CAT_XATTR   "selinux.low_cat"
#define SELINUX_HIGH_CAT_XATTR  "selinux.high_cat"

#define SELINUX_INTERVAL_FIRST_KEY "f"
#define SELINUX_INTERVAL_LAST_KEY  "l"


/*
 * A SELinux complete context
 */
struct selinux_context {
    char *user;
    char *role;
    char *type;
    char *range;
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
split_selinux_context(char *ctx, struct selinux_context *parts)
{
    char *sep1;
    char *sep2;
    char *sep3;

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

    *sep1 = '\0';
    *sep2 = '\0';
    *sep3 = '\0';

    parts->user = ctx;
    parts->role = sep1 + 1;
    parts->type = sep2 + 1;
    parts->range = sep3 + 1;

    return 0;

invalid:
    errno = EINVAL;
    return -1;
}


static int
fill_selinux_pairs(char *ctx, struct rbh_value_pair *pairs,
                   struct rbh_sstack *values)
{
    struct selinux_range parsed_range;
    struct selinux_context parts;
    char *range_copy;
    int count = 0;

    if (fill_string_pair(SELINUX_CONTEXT_XATTR, ctx,
                         &pairs[count++], values))
        return -1;

    if (split_selinux_context(ctx, &parts))
        return -1;

    if (fill_string_pair(SELINUX_USER_XATTR, parts.user,
                         &pairs[count++], values))
        return -1;

    if (fill_string_pair(SELINUX_ROLE_XATTR, parts.role,
                         &pairs[count++], values))
        return -1;

    if (fill_string_pair(SELINUX_TYPE_XATTR, parts.type,
                         &pairs[count++], values))
        return -1;

    if (fill_string_pair(SELINUX_RANGE_XATTR, parts.range,
                         &pairs[count++], values))
        return -1;

    range_copy = xstrdup(parts.range);

    if (selinux_parse_range(range_copy, &parsed_range)) {
        free(range_copy);
        return count;
    }

    free(range_copy);

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

    if (pairs_count < 8) {
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


void
rbh_selinux_helper(__attribute__((unused)) const char *backend,
                  __attribute__((unused)) struct rbh_config *config,
                  char **predicate_helper, char **directive_helper)
{
    int rc;
    rc = asprintf(predicate_helper,
        " - SELinux:\n"
        "   -selinux-context CONTEXT\n"
        "                         filter entries based on their complete\n"
        "                         SELinux context.\n"
        "   -selinux-user USER    filter entries based on their SELinux user\n"
        "   -selinux-role ROLE    filter entries based on their SELinux role\n"
        "   -selinux-type TYPE    filter entries based on their SELinux type\n"
        "   -selinux-range RANGE  filter entries based on their complete\n"
        "                         SELinux range.\n"
        "   -selinux-range-dominates LEVEL\n"
        "                         filter entries whose SELinux high level\n"
        "                         dominates LEVEL.\n");


    if (rc == -1)
        *predicate_helper = NULL;

    rc = asprintf(directive_helper,
        "  - SELinux, directive category 'Z':\n"
        "    %%RZc Complete SELinux context.\n"
        "    %%RZu SELinux user.\n"
        "    %%RZr SELinux role.\n"
        "    %%RZt SELinux type.\n"
        "    %%RZR SELinux range.\n");

    if (rc == -1)
        *directive_helper = NULL;
}
