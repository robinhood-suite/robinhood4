/* This file is part of RobinHood 4
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

#include <lustre/lustreapi.h>

#include <robinhood/statx.h>

#include <robinhood/backend.h>
#include <rbh-find/filters.h>
#include <rbh-find/utils.h>

#include "filters.h"

static const struct rbh_filter_field predicate2filter_field[] = {
    [LPRED_EXPIRED - LPRED_MIN] =   {.fsentry = RBH_FP_INODE_XATTRS,
                                     .xattr = "user.ccc_expiration_date"},
    [LPRED_FID - LPRED_MIN] =       {.fsentry = RBH_FP_NAMESPACE_XATTRS,
                                     .xattr = "fid"},
    [LPRED_HSM_STATE - LPRED_MIN] = {.fsentry = RBH_FP_NAMESPACE_XATTRS,
                                     .xattr = "hsm_state"},
    [LPRED_OST_INDEX - LPRED_MIN] = {.fsentry = RBH_FP_NAMESPACE_XATTRS,
                                     .xattr = "ost"},
};

static enum hsm_states
str2hsm_states(const char *hsm_state)
{
    switch (hsm_state[0]) {
    case 'a':
        if (strcmp(&hsm_state[1], "rchived"))
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        return HS_ARCHIVED;
    case 'd':
        if (strcmp(&hsm_state[1], "irty"))
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        return HS_DIRTY;
    case 'e':
        if (strcmp(&hsm_state[1], "xists"))
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        return HS_EXISTS;
    case 'l':
        if (strcmp(&hsm_state[1], "ost"))
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        return HS_LOST;
    case 'n':
        if (hsm_state[1] != 'o')
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        switch (hsm_state[2]) {
        case 'a':
            if (strcmp(&hsm_state[3], "rchive"))
                error(EX_USAGE, 0, "invalid argument to -hsm-state");
            return HS_NOARCHIVE;
        case 'n':
            if (hsm_state[3] != 'e' || hsm_state[4] != 0)
                error(EX_USAGE, 0, "invalid argument to -hsm-state");
            return HS_NONE;
        case 'r':
            if (strcmp(&hsm_state[3], "elease"))
                error(EX_USAGE, 0, "invalid argument to -hsm-state");
            return HS_NORELEASE;
        default:
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
            __builtin_unreachable();
        }
    case 'r':
        if (strcmp(&hsm_state[1], "eleased"))
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        return HS_RELEASED;
    }

    error(EX_USAGE, 0, "unknown hsm-state: `%s'", hsm_state);
    __builtin_unreachable();
}

struct rbh_filter *
hsm_state2filter(const char *hsm_state)
{
    enum hsm_states state = str2hsm_states(hsm_state);
    struct rbh_filter *filter;

    if (state == HS_NONE) {
        struct rbh_filter *file_filter;

        file_filter = filetype2filter("f");
        if (file_filter == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "hsm_state2filter");

        filter = rbh_filter_exists_new(
                &predicate2filter_field[LPRED_HSM_STATE - LPRED_MIN]);
        if (filter == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "hsm_state2filter");

        filter = filter_not(filter);
        if (filter == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "hsm_state2filter");

        filter = filter_and(file_filter, filter);
    } else {
        filter = rbh_filter_compare_uint32_new(
                RBH_FOP_BITS_ANY_SET,
                &predicate2filter_field[LPRED_HSM_STATE - LPRED_MIN], state
                );
    }

    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "hsm_state2filter");

    return filter;
}

static bool
check_balanced_braces(const char *fid)
{
    const char last = *(fid + strlen(fid) - 1);
    const char *iter = fid;
    int nb_braces = 0;

    while (*iter) {
        switch (*iter) {
        case '[':
            nb_braces++;
            break;
        case ']':
            if (nb_braces <= 0)
                return false;

            nb_braces--;
            break;
        default:
            break;
        }
        iter++;
    }

    return nb_braces == 0 &&
        (*fid == '[' ? last == ']' : true);
}

struct rbh_filter *
fid2filter(const char *fid)
{
    struct rbh_filter *filter;
    struct lu_fid lu_fid;
    char *endptr;
    int rc;

#ifdef HAVE_FID_PARSE
    rc = llapi_fid_parse(fid, &lu_fid, &endptr);
    if (rc || *endptr != '\0' || !check_balanced_braces(fid))
        error(EX_USAGE, 0, "invalid fid parsing: %s", strerror(-rc));
#else
    char *fid_format;
    int n_chars;

    if (*fid == '[')
        fid_format = "[" SFID "%n]";
    else
        fid_format = SFID "%n";

    rc = sscanf(fid, fid_format, RFID(&lu_fid), &n_chars);
    if (rc != 3)
        error(EX_USAGE, 0, "invalid fid parsing");

    switch (*fid) {
    case '[':
        if (fid[n_chars] != ']' || fid[n_chars + 1] != '\0')
            error(EX_USAGE, 0, "invalid fid parsing");
        break;
    default:
        if (fid[n_chars] != '\0')
            error(EX_USAGE, 0, "invalid fid parsing");
        break;
    }
#endif

    filter = rbh_filter_compare_binary_new(
            RBH_FOP_EQUAL, &predicate2filter_field[LPRED_FID - LPRED_MIN],
            (char *) &lu_fid, sizeof(lu_fid)
            );
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "fid2filter");

    return filter;
}

struct rbh_filter *
ost_index2filter(const char *ost_index)
{
    struct rbh_value index_value = {
        .type = RBH_VT_UINT64,
    };
    struct rbh_filter *filter;
    uint64_t index;
    int rc;

    if (!isdigit(*ost_index))
        error(EX_USAGE, 0, "invalid ost index: `%s'", ost_index);

    rc = str2uint64_t(ost_index, &index);
    if (rc)
        error(EX_USAGE, errno, "invalid ost index: `%s'", ost_index);

    index_value.uint64 = index;
    filter = rbh_filter_compare_sequence_new(
            RBH_FOP_IN, &predicate2filter_field[LPRED_OST_INDEX - LPRED_MIN],
            &index_value, 1
            );
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "ost_index2filter");

    return filter;
}

struct rbh_filter *
expired2filter()
{
    struct rbh_filter *filter_expiration_date;
    uint64_t now;

    now = time(NULL);

    filter_expiration_date = rbh_filter_compare_uint64_new(
        RBH_FOP_LOWER_OR_EQUAL,
        &predicate2filter_field[LPRED_EXPIRED - LPRED_MIN],
        now);
    if (!filter_expiration_date)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_uint64_new");

    return filter_expiration_date;
}

struct rbh_filter *
expired_at2filter(const char *expired)
{
    const struct rbh_filter_field predicate =
        predicate2filter_field[LPRED_EXPIRED - LPRED_MIN];
    struct rbh_filter *filter_expiration_date;
    struct rbh_filter *filter_inf;
    uint64_t inf = UINT64_MAX;

    if (!strcmp(expired, "inf")) {
        filter_inf = rbh_filter_compare_uint64_new(RBH_FOP_EQUAL, &predicate,
                                                   inf);
        if (!filter_inf)
            error(EXIT_FAILURE, errno, "rbh_filter_compare_uint64_new");

        return filter_inf;
    }

    if (!isdigit(expired[0]) && expired[0] != '+' && expired[0] != '-')
        error(EXIT_FAILURE, errno, "invalid argument `%s' to `%s'", expired,
              lustre_predicate2str(LPRED_EXPIRED_AT));

    if ((expired[0] == '+' || expired[0] == '-') && !isdigit(expired[1]))
        error(EXIT_FAILURE, errno, "invalid argument `%s' to `%s'", expired,
              lustre_predicate2str(LPRED_EXPIRED_AT));

    filter_expiration_date = epoch2filter(&predicate, expired);
    if (!filter_expiration_date)
        error(EXIT_FAILURE, errno, "epoch2filter");

    /* If we want to check all the entries that will be expired after a certain
     * time, do not include those that have an infinite expiration date, as it
     * internally is equivalent to an expiration date set to UINT64_MAX.
     */
    filter_inf = rbh_filter_compare_uint64_new(RBH_FOP_EQUAL, &predicate, inf);
    if (!filter_inf)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_uint64_new");

    return filter_and(filter_not(filter_inf), filter_expiration_date);
}
