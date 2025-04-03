/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <string.h>

#include <robinhood/filter.h>

#include "parser.h"

int
str2lustre_predicate(const char *string)
{
    assert(string[0] == '-');

    switch (string[1]) {
    case 'c':
        if (strncmp(&string[2], "omp-", 4))
            break;

        switch (string[6]) {
        case 'e':
            if (!strcmp(&string[7], "nd"))
                return LPRED_COMP_END;

            break;
        case 's':
            if (!strcmp(&string[7], "tart"))
                return LPRED_COMP_START;

            break;
        }
        break;
    case 'f':
        if (strcmp(&string[2], "id") == 0)
            return LPRED_FID;
        break;
    case 'h':
        if (strcmp(&string[2], "sm-state") == 0)
            return LPRED_HSM_STATE;
        break;
    case 'i':
        if (strcmp(&string[2], "pool") == 0)
            return LPRED_IPOOL;
        break;
    case 'l':
        if (strcmp(&string[2], "ayout-pattern") == 0)
            return LPRED_LAYOUT_PATTERN;
        break;
    case 'm':
        if (strncmp(&string[2], "dt-", 3))
            break;

        if (strcmp(&string[5], "count") == 0)
            return LPRED_MDT_COUNT;

        if (strcmp(&string[5], "index") == 0)
            return LPRED_MDT_INDEX;
        break;
    case 'o':
        if (strcmp(&string[2], "st") == 0)
            return LPRED_OST_INDEX;
        break;
    case 'p':
        if (strcmp(&string[2], "ool") == 0)
            return LPRED_POOL;
        break;
    case 's':
        if (strcmp(&string[2], "tripe-count") == 0)
            return LPRED_STRIPE_COUNT;
        else if (strcmp(&string[2], "tripe-size") == 0)
            return LPRED_STRIPE_SIZE;
        break;
    }

    return -1;
}

enum rbh_parser_token
rbh_lustre_check_valid_token(const char *token)
{
    if (str2lustre_predicate(token) != -1)
        return RBH_TOKEN_PREDICATE;

    return RBH_TOKEN_UNKNOWN;
}

static const char *__lustre_predicate2str[] = {
    [LPRED_COMP_END]       = "comp-end",
    [LPRED_COMP_START]     = "comp-start",
    [LPRED_FID]            = "fid",
    [LPRED_HSM_STATE]      = "hsm-state",
    [LPRED_IPOOL]          = "ipool",
    [LPRED_LAYOUT_PATTERN] = "layout-pattern",
    [LPRED_MDT_COUNT]      = "mdt-count",
    [LPRED_MDT_INDEX]      = "mdt-index",
    [LPRED_OST_INDEX]      = "ost",
    [LPRED_POOL]           = "pool",
    [LPRED_STRIPE_COUNT]   = "stripe-count",
    [LPRED_STRIPE_SIZE]    = "stripe-size",
};

const char *
lustre_predicate2str(int predicate)
{
    assert(LPRED_MIN <= predicate && predicate < LPRED_MAX);

    return __lustre_predicate2str[predicate - LPRED_MIN];
}
