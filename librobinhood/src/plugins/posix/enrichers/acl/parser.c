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
str2acl_predicate(const char *string)
{
    assert(string[0] == '-');

    switch (string[1]) {
    case 'a':
        if (strcmp(&string[2], "cl-user") == 0)
            return APRED_USER;

        if (strcmp(&string[2], "cl-group") == 0)
            return APRED_GROUP;

        if (strcmp(&string[2], "cl-default-user") == 0)
            return APRED_DEFAULT_USER;

        if (strcmp(&string[2], "cl-default-group") == 0)
            return APRED_DEFAULT_GROUP;
        break;
    }

    return -1;
}

enum rbh_parser_token
rbh_acl_check_valid_token(const char *token)
{
    if (str2acl_predicate(token) != -1)
        return RBH_TOKEN_PREDICATE;

    return RBH_TOKEN_UNKNOWN;
}

static const char *__acl_predicate2str[] = {
    [APRED_USER]             = "acl-user",
    [APRED_GROUP]            = "acl-group",
    [APRED_DEFAULT_USER]     = "acl-default-user",
    [APRED_DEFAULT_GROUP]    = "acl-default-group",
};

const char *
acl_predicate2str(int predicate)
{
    assert(APRED_MIN <= predicate && predicate <= APRED_MAX);

    return __acl_predicate2str[predicate];
}
