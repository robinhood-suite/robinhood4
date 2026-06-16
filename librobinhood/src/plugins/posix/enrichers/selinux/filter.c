/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <assert.h>
#include <ctype.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <sysexits.h>

#include <robinhood/utils.h>
#include <robinhood/filter.h>
#include <robinhood/filters/core.h>

#include "parser.h"
#include "range_parser.h"
#include "selinux_internals.h"


static const struct rbh_filter_field predicate2filter_field[] = {
    [SPRED_SELINUX_CTX]       = {.fsentry = RBH_FP_INODE_XATTRS,
                                 .xattr = "selinux.context"},
    [SPRED_SELINUX_USER]      = {.fsentry = RBH_FP_INODE_XATTRS,
                                 .xattr = "selinux.user"},
    [SPRED_SELINUX_ROLE]      = {.fsentry = RBH_FP_INODE_XATTRS,
                                 .xattr = "selinux.role"},
    [SPRED_SELINUX_TYPE]      = {.fsentry = RBH_FP_INODE_XATTRS,
                                 .xattr = "selinux.type"},
    [SPRED_SELINUX_RANGE]     = {.fsentry = RBH_FP_INODE_XATTRS,
                                 .xattr = "selinux.range"},
    [SELINUX_HIGH_SENS_FIELD] = {.fsentry = RBH_FP_INODE_XATTRS,
                                 .xattr = "selinux.high_sens"},
    [SELINUX_HIGH_CAT_FIELD]  = {.fsentry = RBH_FP_INODE_XATTRS,
                                 .xattr = "selinux.high_cat"},
    [INTERVAL_LAST_FIELD]     = {.fsentry = RBH_FP_INODE_XATTRS,
                                 .xattr = "l"},
    [INTERVAL_FIRST_FIELD]    = {.fsentry = RBH_FP_INODE_XATTRS,
                                 .xattr = "f"},
};

static struct rbh_filter *
string_predicate2filter(int predicate, const char *arg)
{
    const struct rbh_filter_field *field = &predicate2filter_field[predicate];
    struct rbh_filter *filter = NULL;

    filter = rbh_filter_compare_string_new(RBH_FOP_EQUAL, field, arg);

    if (filter == NULL)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_string_new");

    return filter;
}


static struct rbh_filter *
interval_cover_filter(const struct selinux_interval *interval)
{
    const struct rbh_filter *subfilters[2];
    struct rbh_filter *filter;
    struct rbh_filter *first;
    struct rbh_filter *last;

    first = rbh_filter_compare_int32_new(
                RBH_FOP_LOWER_OR_EQUAL,
                &predicate2filter_field[INTERVAL_FIRST_FIELD],
                interval->first);

    if (first == NULL)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_int32_new");

    last = rbh_filter_compare_int32_new(
               RBH_FOP_GREATER_OR_EQUAL,
               &predicate2filter_field[INTERVAL_LAST_FIELD],
               interval->last);

    if (last == NULL)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_int32_new");

    subfilters[0] = first;
    subfilters[1] = last;

    filter = rbh_filter_array_elemmatch_new(
                &predicate2filter_field[SELINUX_HIGH_CAT_FIELD],
                subfilters, 2, true);

    if (filter == NULL)
        error(EXIT_FAILURE, errno, "rbh_filter_array_elemmatch_new");

    free(first);
    free(last);

    return filter;
}


static struct rbh_filter *
selinux_dominates_filter(const char *arg)
{
    struct selinux_level level;
    struct rbh_filter *filter;
    char *copy;

    copy = xstrdup(arg);

    if (selinux_parse_level(copy, &level)) {
        free(copy);
        error(EX_USAGE, errno, "invalid SELinux level `%s'", arg);
    }

    free(copy);

    filter = rbh_filter_compare_int32_new(
                RBH_FOP_GREATER_OR_EQUAL,
                &predicate2filter_field[SELINUX_HIGH_SENS_FIELD],
                level.sens);

    if (filter == NULL)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_int32_new");

    for (size_t i = 0; i < level.intervals_count; i++) {
        struct rbh_filter *cat_filter;

        cat_filter = interval_cover_filter(&level.intervals[i]);
        filter = rbh_filter_and(filter, cat_filter);
    }

    return filter;
}


struct rbh_filter *
rbh_selinux_build_filter(struct filters_context *context, int *index)
{
    char **argv = context->argv;
    struct rbh_filter *filter;
    int argc = context->argc;
    int i = *index;
    int predicate;

    predicate = str2selinux_predicate(argv[i]);

    if (i + 1 >= argc)
        error(EX_USAGE, 0, "missing argument to `%s'", argv[i]);

    /* In the following block, functions should call error() themselves rather
     * than returning.
     *
     * Errors are most likely fatal (not recoverable), and this allows for
     * precise and meaningul error messages.
     */
    switch (predicate) {
        case SPRED_SELINUX_CTX:
        case SPRED_SELINUX_USER:
        case SPRED_SELINUX_ROLE:
        case SPRED_SELINUX_TYPE:
        case SPRED_SELINUX_RANGE:
            filter = string_predicate2filter(predicate, argv[++i]);
        break;
        case SPRED_SELINUX_RANGE_DOMINATES:
            filter = selinux_dominates_filter(argv[++i]);
            break;
        default:
            error(EX_USAGE, 0, "invalid filter found `%s'", argv[i]);
    }
    assert(filter != NULL);

    *index = i;
    return filter;
}
