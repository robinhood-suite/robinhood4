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
    case 'f':
        if (strcmp(&string[2], "id") == 0)
            return LPRED_FID;
        break;
    case 'h':
        if (strcmp(&string[2], "sm-state") == 0)
            return LPRED_HSM_STATE;
        break;
    case 'l':
        if (strcmp(&string[2], "ayout") == 0)
            return LPRED_LAYOUT;
        break;
    case 'o':
        if (strcmp(&string[2], "st") == 0)
            return LPRED_OST_INDEX;
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

static const char *__lustre_predicate2str[] = {
    [LPRED_FID - LPRED_MIN]           = "fid",
    [LPRED_HSM_STATE - LPRED_MIN]     = "hsm-state",
    [LPRED_OST_INDEX - LPRED_MIN]     = "ost",
    [LPRED_LAYOUT - LPRED_MIN]        = "layout",
    [LPRED_STRIPE_COUNT - LPRED_MIN]  = "stripe-count",
    [LPRED_STRIPE_SIZE - LPRED_MIN]   = "stripe-size",
};

const char *
lustre_predicate2str(int predicate)
{
    if (LPRED_MIN <= predicate && predicate < LPRED_MAX)
        return __lustre_predicate2str[predicate - LPRED_MIN];

    return predicate2str(predicate);
}
