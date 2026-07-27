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
#include <robinhood/statx.h>
#include <robinhood/filters/core.h>

#include "parser.h"

struct acl_subject {
    uint32_t uid;
    uint32_t *gids;
    size_t gid_count;
};

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
    [ACL_PERMS_FIELD]     = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "p"},
    [ACL_GROUP_FIELD]     = {.fsentry = RBH_FP_INODE_XATTRS,
                             .xattr = "acl.access.group"},
    [STATX_UID_FIELD]     = {.fsentry = RBH_FP_STATX,
                             .statx = RBH_STATX_UID},
    [STATX_GID_FIELD]     = {.fsentry = RBH_FP_STATX,
                             .statx = RBH_STATX_GID},
    [STATX_MODE_FIELD]    = {.fsentry = RBH_FP_STATX,
                             .statx = RBH_STATX_MODE},
};

static uint32_t
parse_acl_id(const char *arg)
{
    uint64_t id;

    if (str2uint64_t(arg, &id) || id > UINT32_MAX)
        error(EX_USAGE, 0, "invalid ACL identifier '%s'", arg);

    return id;
}


/*
 * Match a named ACL entry by ID and permissions if a
 * non-negative value is provided.
 */
static struct rbh_filter *
acl_named_entry2filter(const struct rbh_filter_field *field, uint32_t id,
                       int perms)
{
    const struct rbh_filter *subfilters[2];
    struct rbh_filter *perms_filter = NULL;
    struct rbh_filter *id_filter;
    struct rbh_filter *filter;
    size_t count = 1;

    id_filter = rbh_filter_compare_uint32_new(
                    RBH_FOP_EQUAL,
                    &predicate2filter_field[ACL_ID_FIELD],
                    id);
    if (id_filter == NULL)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_uint32_new");

    subfilters[0] = id_filter;

    if (perms >= 0) {
        perms_filter = rbh_filter_compare_uint32_new(
                RBH_FOP_BITS_ALL_SET,
                &predicate2filter_field[ACL_PERMS_FIELD],
                perms);
        if (perms_filter == NULL)
            error(EXIT_FAILURE, errno, "rbh_filter_compare_uint32_new");

       subfilters[count++] = perms_filter;
    }

    filter = rbh_filter_array_elemmatch_new(field, subfilters, count, true);
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "rbh_filter_array_elemmatch_new");

    free(id_filter);
    free(perms_filter);

    return filter;
}

static void
parse_acl_subject(char *string, struct acl_subject *subject)
{
    char *group;
    char *comma;
    char *colon;

    memset(subject, 0, sizeof(*subject));

    colon = strchr(string, ':');
    if (colon == NULL || colon == string || colon[1] == '\0')
        error(EX_USAGE, 0,
              "invalid ACL subject, expected UID:GID[,GID...]");

    *colon = '\0';
    subject->uid = parse_acl_id(string);
    group = colon + 1;

    for (;;) {
        uint32_t *gids;

        comma = strchr(group, ',');
        if (comma != NULL)
            *comma = '\0';

        if (*group == '\0')
            error(EX_USAGE, 0,
                  "invalid ACL subject, expected UID:GID[,GID...]");

        gids = xreallocarray(subject->gids, subject->gid_count + 1,
                            sizeof(*subject->gids));

        subject->gids = gids;
        subject->gids[subject->gid_count++] = parse_acl_id(group);

        if (comma == NULL)
            break;

        group = comma + 1;
    }
}

static struct rbh_filter *
uid2filter(uint32_t uid)
{
    struct rbh_filter *filter;

    filter = rbh_filter_compare_uint32_new(RBH_FOP_EQUAL,
            &predicate2filter_field[STATX_UID_FIELD],
            uid);
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_uint32_new");

    return filter;
}

static struct rbh_filter *
mode2filter(uint32_t bit)
{
    struct rbh_filter *filter;

    filter = rbh_filter_compare_uint32_new(
            RBH_FOP_BITS_ALL_SET,
            &predicate2filter_field[STATX_MODE_FIELD],
            bit);
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_uint32_new");

    return filter;
}


static struct rbh_filter *
acl_groups2filter(const struct acl_subject *subject, bool named, int perms)
{
    struct rbh_filter *filter = NULL;

    for (size_t i = 0; i < subject->gid_count; i++) {
        struct rbh_filter *current;

        if (named) {
            current = acl_named_entry2filter(
                    &predicate2filter_field[APRED_GROUP],
                    subject->gids[i],
                    perms);
        } else {
            current = rbh_filter_compare_uint32_new(
                    RBH_FOP_EQUAL,
                    &predicate2filter_field[STATX_GID_FIELD],
                    subject->gids[i]);
            if (current == NULL)
                error(EXIT_FAILURE, errno, "rbh_filter_compare_uint32_new");
        }
        filter = filter == NULL ? current : rbh_filter_or(filter, current);
    }
    return filter;
}

static struct rbh_filter *
owner2filter(const struct acl_subject *subject, uint32_t owner_bit)
{
    return rbh_filter_and(uid2filter(subject->uid),
                          mode2filter(owner_bit));
}

/*
 * Match the POSIX ACL group class: the owning group and all matching named
 * groups.
 */
static struct rbh_filter *
group_match2filter(const struct acl_subject *subject, int perms)
{
    struct rbh_filter *owning_group;
    struct rbh_filter *named_groups;

    owning_group = acl_groups2filter(subject, false, -1);
    named_groups = acl_groups2filter(subject, true, perms);

    if (perms >= 0) {
        struct rbh_filter *group_permissions;

        group_permissions = rbh_filter_or(
                rbh_filter_not(
                    rbh_filter_exists_new(
                        &predicate2filter_field[ACL_GROUP_FIELD])),
                rbh_filter_compare_uint32_new(
                    RBH_FOP_BITS_ALL_SET,
                    &predicate2filter_field[ACL_GROUP_FIELD],
                    perms));

        owning_group = rbh_filter_and(owning_group, group_permissions);
    }

    return rbh_filter_or(owning_group, named_groups);
}

/*
 * A named user entry is considered only when the subject is not the file
 * owner. Its effective permissions are the intersection of the entry
 * permissions and the ACL mask stored in the group mode bits.
 */
static struct rbh_filter *
named_user2filter(const struct acl_subject *subject, uint32_t acl_perm,
                  uint32_t group_bit)
{
    struct rbh_filter *not_owner;
    struct rbh_filter *permissions;

    not_owner = rbh_filter_not(uid2filter(subject->uid));

    permissions = rbh_filter_and(
            acl_named_entry2filter(&predicate2filter_field[APRED_USER],
                                   subject->uid,
                                   acl_perm),
            mode2filter(group_bit));

    return rbh_filter_and(not_owner, permissions);
}

/*
 * The group class is considered only if neither the owner entry nor a named
 * user entry matches the subject.
 */
static struct rbh_filter *
group2filter(const struct acl_subject *subject, uint32_t acl_perm,
             uint32_t group_bit)
{
    struct rbh_filter *more_specific;
    struct rbh_filter *permissions;

    more_specific = rbh_filter_not(
            rbh_filter_or(
                uid2filter(subject->uid),
                acl_named_entry2filter(&predicate2filter_field[APRED_USER],
                                       subject->uid,
                                       -1)));

    permissions = rbh_filter_and(
            group_match2filter(subject, acl_perm),
            mode2filter(group_bit));

    return rbh_filter_and(more_specific, permissions);
}

/*
 * The other entry applies only when no owner, named user, owning group or
 * named group entry matches.
 */
static struct rbh_filter *
other2filter(const struct acl_subject *subject, uint32_t other_bit)
{
    struct rbh_filter *filter;

    filter = uid2filter(subject->uid);
    filter = rbh_filter_or(
            filter,
            acl_named_entry2filter(
                &predicate2filter_field[APRED_USER],
                subject->uid, -1));
    filter = rbh_filter_or(filter, group_match2filter(subject, -1));

    return rbh_filter_and(
            rbh_filter_not(filter),
                mode2filter(other_bit));
}


static struct rbh_filter *
acl_access2filter(char *arg, uint32_t acl_perm, uint32_t owner_bit,
                  uint32_t group_bit, uint32_t other_bit)
{
    struct acl_subject subject;
    struct rbh_filter *filter;

    parse_acl_subject(arg, &subject);

    filter = owner2filter(&subject, owner_bit);

    filter = rbh_filter_or(filter,
            named_user2filter(&subject, acl_perm, group_bit));

    filter = rbh_filter_or(filter,
            group2filter(&subject, acl_perm, group_bit));

    filter = rbh_filter_or(filter,
            other2filter(&subject, other_bit));

    free(subject.gids);
    return filter;
}


struct rbh_filter *
rbh_acl_build_filter(struct filters_context *context, int *index)
{
    char **argv = context->argv;
    struct rbh_filter *filter = NULL;
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
            filter = acl_named_entry2filter(&predicate2filter_field[predicate],
                                            parse_acl_id(argv[++i]), -1);
            break;
        case APRED_READABLE:
            filter = acl_access2filter(argv[++i], 4, S_IRUSR, S_IRGRP, S_IROTH);
            break;
        case APRED_WRITABLE:
            filter = acl_access2filter(argv[++i], 2, S_IWUSR, S_IWGRP, S_IWOTH);
            break;
        case APRED_EXECUTABLE:
            filter = acl_access2filter(argv[++i], 1, S_IXUSR, S_IXGRP, S_IXOTH);
            break;

        default:
            error(EX_USAGE, 0, "invalid filter found `%s'", argv[i]);
    }
    assert(filter != NULL);

    *index = i;
    return filter;
}
