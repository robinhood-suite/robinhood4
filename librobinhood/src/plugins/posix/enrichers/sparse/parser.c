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
str2sparse_predicate(const char *string)
{
    assert(string[0] == '-');

    switch (string[1]) {
    case 's':
        if (strcmp(&string[2], "parse") == 0)
            return SPRED_SPARSE;

        break;
    }

    return -1;
}

enum rbh_parser_token
rbh_sparse_check_valid_token(const char *token)
{
    if (str2sparse_predicate(token) != -1)
        return RBH_TOKEN_PREDICATE;

    return RBH_TOKEN_UNKNOWN;
}

static const char *__sparse_predicate2str[] = {
    [SPRED_SPARSE]         = "sparse",
};

const char *
sparse_predicate2str(int predicate)
{
    assert(SPRED_MIN <= predicate && predicate <= SPRED_MAX);

    return __sparse_predicate2str[predicate];
}
