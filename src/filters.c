/* This file is part of rbh-find-lustre
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <ctype.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include <lustre/lustreapi.h>

#include <robinhood/backend.h>
#include <rbh-find/filters.h>

#include "filters.h"

static const struct rbh_filter_field predicate2filter_field[] = {
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

struct rbh_filter *
fid2filter(const char *fid)
{
    struct rbh_filter *filter;
    struct lu_fid lu_fid;
    int rc;

#if HAVE_FID_PARSE
    rc = llapi_fid_parse(fid, &lu_fid, NULL);
    if (rc)
        error(EX_USAGE, 0, "invalid fid parsing: %s", strerror(-rc));
#else
    char *fid_format;
    int n_chars;

    if (*fid == '[')
        fid_format = "[" SFID "]%n";
    else
        fid_format = SFID "%n";

    rc = sscanf(fid, fid_format, RFID(&lu_fid), &n_chars);
    if (rc != 3)
        error(EX_USAGE, 0, "invalid fid parsing");

    if (fid[n_chars] != '\0')
        error(EX_USAGE, 0, "invalid fid parsing");
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
