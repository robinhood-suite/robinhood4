/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <string.h>

#include <robinhood/filter.h>
#include <robinhood/backend.h>

enum rbh_parser_token
rbh_s3_check_valid_token(const char *token)
{
    assert(token[0] == '-');

    switch (token[1]) {
    case 'b':
        if (strcmp(&token[2], "ucket") == 0)
            return RBH_TOKEN_PREDICATE;
        break;
    case 'm':
        if (strcmp(&token[2], "time") == 0)
            return RBH_TOKEN_PREDICATE;
        break;

    case 'n':
        if (strcmp(&token[2], "ame") == 0)
            return RBH_TOKEN_PREDICATE;
        break;

    case 'p':
        if (strcmp(&token[2], "ath") == 0)
            return RBH_TOKEN_PREDICATE;
        break;

    case 's':
        if (strcmp(&token[2], "ize") == 0)
            return RBH_TOKEN_PREDICATE;
        break;
    }

    return RBH_TOKEN_UNKNOWN;
}
