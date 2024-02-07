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
    case 'o':
        if (strcmp(&string[2], "st") == 0)
            return LPRED_OST_INDEX;
        break;
    }

    return str2predicate(string);
}

#define LOCAL(X) (X) - LPRED_MIN
static const char *__lustre_predicate2str[] = {
    [LOCAL(LPRED_EXPIRED)]         = "expired",
    [LOCAL(LPRED_EXPIRED_AT)]      = "expired-at",
    [LOCAL(LPRED_EXPIRATION_DATE)] = "expired",
    [LOCAL(LPRED_FID)]             = "fid",
    [LOCAL(LPRED_HSM_STATE)]       = "hsm-state",
    [LOCAL(LPRED_OST_INDEX)]       = "ost",
};

const char *
lustre_predicate2str(int predicate)
{
    if (LPRED_MIN <= predicate && predicate < LPRED_MAX)
        return __lustre_predicate2str[predicate - LPRED_MIN];

    return predicate2str(predicate);
}
