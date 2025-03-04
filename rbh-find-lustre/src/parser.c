/* This file is part of RobinHood 4
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <stdio.h>
#include <sysexits.h>

#include <rbh-find/parser.h>

#include "parser.h"

int
str2lustre_predicate(const char *string)
{
    assert(string[0] == '-');

    switch (string[1]) {
    case 'c':
        if (strcmp(&string[2], "omp-start") == 0)
            return LPRED_COMP_START;
        break;
    case 'e':
        if (strncmp(&string[2], "xpired", strlen("xpired")) != 0)
            break;

        if (string[strlen("-expired")] == 0)
            return LPRED_EXPIRED;

        if (strcmp(&string[strlen("-expired")], "-at") == 0)
            return LPRED_EXPIRED_AT;
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
        if (strcmp(&string[2], "dt-index") == 0)
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

    return str2predicate(string);
}

#define LOCAL(X) (X) - LPRED_MIN
static const char *__lustre_predicate2str[] = {
    [LOCAL(LPRED_COMP_START)]     = "comp-start",
    [LOCAL(LPRED_EXPIRED)]        = "expired",
    [LOCAL(LPRED_EXPIRED_AT)]     = "expired-at",
    [LOCAL(LPRED_FID)]            = "fid",
    [LOCAL(LPRED_HSM_STATE)]      = "hsm-state",
    [LOCAL(LPRED_IPOOL)]          = "ipool",
    [LOCAL(LPRED_LAYOUT_PATTERN)] = "layout-pattern",
    [LOCAL(LPRED_MDT_INDEX)]      = "mdt-index",
    [LOCAL(LPRED_OST_INDEX)]      = "ost",
    [LOCAL(LPRED_POOL)]           = "pool",
    [LOCAL(LPRED_STRIPE_COUNT)]   = "stripe-count",
    [LOCAL(LPRED_STRIPE_SIZE)]    = "stripe-size",
};

const char *
lustre_predicate2str(int predicate)
{
    if (LPRED_MIN <= predicate && predicate < LPRED_MAX)
        return __lustre_predicate2str[predicate - LPRED_MIN];

    return predicate2str(predicate);
}
