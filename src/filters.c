/* This file is part of rbh-find-lustre
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include <lustre/lustreapi.h>

#include <robinhood/backend.h>
#include <rbh-find/filters.h>

#include "filters.h"

static const struct rbh_filter_field predicate2filter_field[] = {
    [PRED_FID - PRED_MIN] =       {.fsentry = RBH_FP_NAMESPACE_XATTRS,
                                   .xattr = "fid"},
    [PRED_HSM_STATE - PRED_MIN] = {.fsentry = RBH_FP_NAMESPACE_XATTRS,
                                   .xattr = "hsm_state"},
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
    enum rbh_filter_operator op = RBH_FOP_BITS_ANY_SET;
    enum hsm_states state = str2hsm_states(hsm_state);
    struct rbh_filter *filter;

    if (state == HS_NONE)
        op = RBH_FOP_EQUAL;

    filter = rbh_filter_compare_uint32_new(
            op, &predicate2filter_field[PRED_HSM_STATE - PRED_MIN], state
            );
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "hsm_state2filter");

    return filter;
}

struct rbh_filter *
fid2filter(const char *fid)
{
    char buf[FID_NOBRACE_LEN + 1];
    struct rbh_filter *filter;
    struct lu_fid lu_fid;
    int rc;

#if HAVE_FID_PARSE
    rc = llapi_fid_parse(fid, &lu_fid, NULL);
    if (rc)
        error(EX_USAGE, 0, "invalid fid parsing: %s", strerror(-rc));
#else
    rc = sscanf(fid, SFID, RFID(&lu_fid));
    if (rc != 3)
        error(EX_USAGE, 0, "invalid fid parsing");
#endif

    rc = snprintf(buf, sizeof(buf), DFID_NOBRACE, PFID(&lu_fid));
    if (rc < 0)
        error(EX_USAGE, 0, "failed fid allocation: %s", strerror(EINVAL));

    filter = rbh_filter_compare_string_new(
            RBH_FOP_EQUAL, &predicate2filter_field[PRED_FID - PRED_MIN], buf
            );
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "fid2filter");

    return filter;
}
