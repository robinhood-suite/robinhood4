/* This file is part of RobinHood
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
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

#include <robinhood/filter.h>

#include "parser.h"
#include "sparse_internals.h"

static const struct rbh_filter_field predicate2filter_field[] = {
    [SPRED_SPARSE]        = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "sparseness"},
};

static struct rbh_filter *
sparse2filter()
{
    struct rbh_filter *filter = NULL;

    filter = rbh_filter_compare_uint32_new(
        RBH_FOP_STRICTLY_LOWER, &predicate2filter_field[SPRED_SPARSE], 100
    );

    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "sparse2filter");

    return filter;
}

static bool
predicate_has_argument(int predicate)
{
    return predicate != SPRED_SPARSE;
}

struct rbh_filter *
rbh_sparse_build_filter(const char **argv, int argc, int *index,
                        __attribute__((unused)) bool *need_prefetch)
{
    struct rbh_filter *filter;
    int i = *index;
    int predicate;

    predicate = str2sparse_predicate(argv[i]);

    if (predicate_has_argument(predicate) && i + 1 >= argc)
        error(EX_USAGE, 0, "missing argument to `%s'", argv[i]);

    /* In the following block, functions should call error() themselves rather
     * than returning.
     *
     * Errors are most likely fatal (not recoverable), and this allows for
     * precise and meaningul error messages.
     */
    switch (predicate) {
    case SPRED_SPARSE:
        filter = sparse2filter();
        break;
    default:
        error(EX_USAGE, 0, "invalid filter found `%s'", argv[i]);
    }
    assert(filter != NULL);

    *index = i;
    return filter;
}
