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
       case 'm':
        switch (token[2]) {
        case 't':
            if (strcmp(&token[3], "ime") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'n':
        switch (token[2]) {
        case 'a':
            if (strcmp(&token[3], "me") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
    case 'p':
        switch (token[2]) {
        case 'a':
            if (strcmp(&token[3], "th") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 's':
        switch (token[2]) {
        case 'i':
            if (strcmp(&token[3], "ze") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    }

    return RBH_TOKEN_UNKNOWN;
}
