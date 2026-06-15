/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <assert.h>
#include <string.h>

#include <robinhood/filter.h>

#include "parser.h"

int
str2selinux_predicate(const char *string)
{
    assert(string[0] == '-');

    switch (string[1]) {
    case 's':
        if (strcmp(&string[2], "elinux-context") == 0)
            return SPRED_SELINUX_CTX;

        if (strcmp(&string[2], "elinux-user") == 0)
            return SPRED_SELINUX_USER;

        if (strcmp(&string[2], "elinux-role") == 0)
            return SPRED_SELINUX_ROLE;

        if (strcmp(&string[2], "elinux-type") == 0)
            return SPRED_SELINUX_TYPE;

        if (strcmp(&string[2], "elinux-range") == 0)
            return SPRED_SELINUX_RANGE;

        break;
    }

    return -1;
}

enum rbh_parser_token
rbh_selinux_check_valid_token(const char *token)
{
    if (str2selinux_predicate(token) != -1)
        return RBH_TOKEN_PREDICATE;

    return RBH_TOKEN_UNKNOWN;
}

static const char *__selinux_predicate2str[] = {
    [SPRED_SELINUX_CTX]   = "selinux-context",
    [SPRED_SELINUX_USER]  = "selinux-user",
    [SPRED_SELINUX_ROLE]  = "selinux-role",
    [SPRED_SELINUX_TYPE]  = "selinux-type",
    [SPRED_SELINUX_RANGE] = "selinux-range",
};

const char *
selinux_predicate2str(int predicate)
{
    assert(SPRED_MIN <= predicate && predicate <= SPRED_MAX);

    return __selinux_predicate2str[predicate];
}
