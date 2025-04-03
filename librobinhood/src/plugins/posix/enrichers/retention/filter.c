/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <ctype.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <sysexits.h>

#include <robinhood/filter.h>

#include "parser.h"
#include "retention_internals.h"

static const struct rbh_filter_field predicate2filter_field[] = {
    [RPRED_EXPIRED]        = {.fsentry = RBH_FP_INODE_XATTRS,
                              .xattr = "trusted.expiration_date"},
};

static struct rbh_filter *
expired2filter()
{
    struct rbh_filter *filter_expiration_date;
    uint64_t now;

    now = time(NULL);

    filter_expiration_date = rbh_filter_compare_uint64_new(
        RBH_FOP_LOWER_OR_EQUAL,
        &predicate2filter_field[RPRED_EXPIRED],
        now);
    if (!filter_expiration_date)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_uint64_new");

    return filter_expiration_date;
}

static struct rbh_filter *
expired_at2filter(const char *expired)
{
    const struct rbh_filter_field *predicate =
        &predicate2filter_field[RPRED_EXPIRED];
    struct rbh_filter *filter_expiration_date;
    struct rbh_filter *filter_inf;
    int64_t inf = INT64_MAX;

    if (!strcmp(expired, "inf")) {
        filter_inf = rbh_filter_compare_uint64_new(RBH_FOP_EQUAL, predicate,
                                                   inf);
        if (!filter_inf)
            error(EXIT_FAILURE, errno, "rbh_filter_compare_uint64_new");

        return filter_inf;
    }

    if (!isdigit(expired[0]) && expired[0] != '+' && expired[0] != '-')
        error(EXIT_FAILURE, errno, "invalid argument `%s' to `%s'", expired,
              retention_predicate2str(RPRED_EXPIRED_AT));

    if ((expired[0] == '+' || expired[0] == '-') && !isdigit(expired[1]))
        error(EXIT_FAILURE, errno, "invalid argument `%s' to `%s'", expired,
              retention_predicate2str(RPRED_EXPIRED_AT));

    filter_expiration_date = rbh_epoch2filter(predicate, expired);
    if (!filter_expiration_date)
        error(EXIT_FAILURE, errno, "epoch2filter");

    /* If we want to check all the entries that will be expired after a certain
     * time, do not include those that have an infinite expiration date, as it
     * internally is equivalent to an expiration date set to INT64_MAX.
     */
    filter_inf = rbh_filter_compare_uint64_new(RBH_FOP_EQUAL, predicate, inf);
    if (!filter_inf)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_uint64_new");

    return rbh_filter_and(rbh_filter_not(filter_inf), filter_expiration_date);
}

static bool
predicate_has_argument(int predicate)
{
    return predicate != RPRED_EXPIRED;
}

struct rbh_filter *
rbh_retention_build_filter(const char **argv, int argc, int *index,
                           __attribute__((unused)) bool *need_prefetch)
{
    struct rbh_filter *filter;
    int i = *index;
    int predicate;

    predicate = str2retention_predicate(argv[i]);

    if (predicate_has_argument(predicate) && i + 1 >= argc)
        error(EX_USAGE, 0, "missing argument to `%s'", argv[i]);

    /* In the following block, functions should call error() themselves rather
     * than returning.
     *
     * Errors are most likely fatal (not recoverable), and this allows for
     * precise and meaningul error messages.
     */
    switch (predicate) {
    case RPRED_EXPIRED:
        filter = expired2filter();
        break;
    case RPRED_EXPIRED_AT:
        filter = expired_at2filter(argv[++i]);
        break;
    default:
        error(EX_USAGE, 0, "invalid filter found `%s'", argv[i]);
    }
    assert(filter != NULL);

    *index = i;
    return filter;
}
