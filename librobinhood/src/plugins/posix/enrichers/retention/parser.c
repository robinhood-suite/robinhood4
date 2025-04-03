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
str2retention_predicate(const char *string)
{
    assert(string[0] == '-');

    switch (string[1]) {
    case 'e':
        if (strncmp(&string[2], "xpired", strlen("xpired")) != 0)
            break;

        if (string[strlen("-expired")] == 0)
            return RPRED_EXPIRED;

        if (strcmp(&string[strlen("-expired")], "-at") == 0)
            return RPRED_EXPIRED_AT;
        break;
    }

    return -1;
}

enum rbh_parser_token
rbh_retention_check_valid_token(const char *token)
{
    if (str2retention_predicate(token) != -1)
        return RBH_TOKEN_PREDICATE;

    return RBH_TOKEN_UNKNOWN;
}

static const char *__retention_predicate2str[] = {
    [RPRED_EXPIRED]        = "expired",
    [RPRED_EXPIRED_AT]     = "expired-at",
};

const char *
retention_predicate2str(int predicate)
{
    assert(RPRED_MIN <= predicate && predicate < RPRED_MAX);

    return __retention_predicate2str[predicate];
}
