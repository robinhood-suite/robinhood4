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
#include <sysexits.h>

#include <rbh-find/parser.h>

#include "parser.h"

int
str2lustre_predicate(const char *string)
{
    assert(string[0] == '-');

    if (strcmp(&string[1], "hsm-state") == 0)
        return PRED_HSM_STATE;

    return str2predicate(string);
}

static const char *__lustre_predicate2str[] = {
    [PRED_HSM_STATE]  = "hsm-state",
};

const char *
lustre_predicate2str(int predicate)
{
    if (PRED_MIN <= predicate && predicate < PRED_MAX)
        return __lustre_predicate2str[predicate];

    return predicate2str(predicate);
}
