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

static const struct rbh_filter_field predicate2filter_field[] = {
    [APRED_USER]          = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "acl.access.users"},
    [APRED_GROUP]         = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "acl.access.groups"},
    [APRED_DEFAULT_USER]  = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "acl.default.users"},
    [APRED_DEFAULT_GROUP] = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "acl.default.groups"},
    [ACL_ID_FIELD]        = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "id"},
};

static uint32_t
parse_acl_id(const char *arg)
{
    uint64_t id;

    if (str2uint64_t(arg, &id) || id > UINT32_MAX)
        error(EX_USAGE, 0, "invalid ACL identifier '%s'", arg);

    return id;
}

static struct rbh_filter *
acl_named_entry2filter(int predicate, const char* arg)
{
    const struct rbh_filter *subfilters[1];
    struct rbh_filter *id_filter;
    struct rbh_filter *filter;
    uint32_t id;

    id = parse_acl_id(arg);

    id_filter = rbh_filter_compare_uint32_new(RBH_FOP_EQUAL,
                                              &predicate2filter_field[ACL_ID_FIELD],
                                              id);
    if (id_filter == NULL)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_uint32_new");

    subfilters[0] = id_filter;

    filter = rbh_filter_array_elemmatch_new(&predicate2filter_field[predicate],
                                            subfilters, 1, true);
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "rbh_filter_array_elemmatch_new");

    free(id_filter);

    return filter;
}


struct rbh_filter *
rbh_acl_build_filter(struct filters_context *context, int *index)
{
    char **argv = context->argv;
    struct rbh_filter *filter;
    int argc = context->argc;
    int i = *index;
    int predicate;

    predicate = str2acl_predicate(argv[i]);

    if (i + 1 >= argc)
        error(EX_USAGE, 0, "missing argument to `%s'", argv[i]);

    /* In the following block, functions should call error() themselves rather
     * than returning.
     *
     * Errors are most likely fatal (not recoverable), and this allows for
     * precise and meaningul error messages.
     */
    switch (predicate) {
        case APRED_USER:
        case APRED_GROUP:
        case APRED_DEFAULT_USER:
        case APRED_DEFAULT_GROUP:
            filter = acl_named_entry2filter(predicate, argv[++i]);
            break;
        default:
            error(EX_USAGE, 0, "invalid filter found `%s'", argv[i]);
    }
    assert(filter != NULL);

    *index = i;
    return filter;
}
