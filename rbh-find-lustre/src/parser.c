/* This file is part of rbh-find-lustre
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
    case 'o':
        if (strcmp(&string[2], "st") == 0)
            return LPRED_OST_INDEX;
        break;
    }

    return str2predicate(string);
}

static const char *__lustre_predicate2str[] = {
    [LPRED_FID - LPRED_MIN]  = "fid",
    [LPRED_HSM_STATE - LPRED_MIN]  = "hsm-state",
    [LPRED_OST_INDEX - LPRED_MIN]  = "ost",
};

const char *
lustre_predicate2str(int predicate)
{
    if (LPRED_MIN <= predicate && predicate < LPRED_MAX)
        return __lustre_predicate2str[predicate - LPRED_MIN];

    return predicate2str(predicate);
}
