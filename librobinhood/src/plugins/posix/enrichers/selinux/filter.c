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

#include <robinhood/filter.h>
#include <robinhood/filters/core.h>

#include "parser.h"
#include "selinux_internals.h"

static const struct rbh_filter_field predicate2filter_field[] = {
    [SPRED_SELINUX_CTX]   = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "selinux.context"},
    [SPRED_SELINUX_USER]  = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "selinux.user"},
    [SPRED_SELINUX_ROLE]  = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "selinux.role"},
    [SPRED_SELINUX_TYPE]  = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "selinux.type"},
    [SPRED_SELINUX_RANGE] = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "selinux.range"},
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

static bool
predicate_has_argument(int predicate)
{
    switch(predicate) {
        case SPRED_SELINUX_CTX:
        case SPRED_SELINUX_USER:
        case SPRED_SELINUX_ROLE:
        case SPRED_SELINUX_TYPE:
        case SPRED_SELINUX_RANGE:
            return true;
    
        default:
            return false;
    }
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

    if (predicate_has_argument(predicate) && i + 1 >= argc)
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
        default:
            error(EX_USAGE, 0, "invalid filter found `%s'", argv[i]);
    }
    assert(filter != NULL);

    *index = i;
    return filter;
}
