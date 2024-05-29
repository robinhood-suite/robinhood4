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
    case 'b':
        if (strcmp(&string[2], "egin") == 0)
            return LPRED_BEGIN;
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
    case 'o':
        if (strcmp(&string[2], "st") == 0)
            return LPRED_OST_INDEX;
        break;
    case 'p':
        if (strcmp(&string[2], "ool") == 0)
            return LPRED_POOL;
        break;
    }

    return str2predicate(string);
}

static const char *__lustre_predicate2str[] = {
    [LPRED_BEGIN - LPRED_MIN]  = "begin",
    [LPRED_FID - LPRED_MIN]  = "fid",
    [LPRED_HSM_STATE - LPRED_MIN]  = "hsm-state",
    [LPRED_IPOOL - LPRED_MIN]  = "ipool",
    [LPRED_OST_INDEX - LPRED_MIN]  = "ost",
    [LPRED_POOL - LPRED_MIN]  = "pool",
};

const char *
lustre_predicate2str(int predicate)
{
    if (LPRED_MIN <= predicate && predicate < LPRED_MAX)
        return __lustre_predicate2str[predicate - LPRED_MIN];

    return predicate2str(predicate);
}
