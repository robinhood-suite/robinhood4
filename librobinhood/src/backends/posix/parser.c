/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>

#include "robinhood/backends/posix.h"

enum rbh_parser_token
rbh_posix_check_valid_token(const char *token)
{
    assert(token[0] == '-');

    switch (token[1]) {
    case 'a':
        switch (token[2]) {
        case 'm':
            if (strcmp(&token[3], "in") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'n':
            if (strcmp(&token[3], "ewer") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 't':
            if (strcmp(&token[3], "ime") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'b':
        switch (token[2]) {
        case 'l':
            if (strcmp(&token[3], "ocks") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'm':
            if (strcmp(&token[3], "in") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 't':
            if (strcmp(&token[3], "ime") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'c':
        switch (token[2]) {
        case 'm':
            if (strcmp(&token[3], "in") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'n':
            if (strcmp(&token[3], "ewer") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'o':
            if (strcmp(&token[3], "ntext") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 't':
            if (strcmp(&token[3], "ime") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'e':
        switch (token[2]) {
        case 'm':
            if (strcmp(&token[3], "pty") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'x':
            if (strcmp(&token[3], "ecutable") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'f':
        switch (token[2]) {
        case 'a':
            if (strcmp(&token[3], "lse") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 's':
            if (strcmp(&token[3], "type") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'g':
        switch (token[2]) {
        case 'i':
            if (strcmp(&token[3], "d") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'r':
            if (strcmp(&token[3], "oup") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'i':
        switch (token[2]) {
        case 'l':
            if (strcmp(&token[3], "name") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'n':
            switch (token[3]) {
            case 'a':
                if (strcmp(&token[4], "me") == 0)
                    return RBH_TOKEN_PREDICATE;
                break;
            case 'u':
                if (strcmp(&token[4], "m") == 0)
                    return RBH_TOKEN_PREDICATE;
                break;
            }
            break;
        case 'p':
            if (strcmp(&token[3], "ath") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'r':
            if (strcmp(&token[3], "egex") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'w':
            if (strcmp(&token[3], "holename") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'l':
        switch (token[2]) {
        case 'i':
            if (strcmp(&token[3], "nks") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'n':
            if (strcmp(&token[3], "ame") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'm':
        switch (token[2]) {
        case 'm':
            if (strcmp(&token[3], "in") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
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
        case 'e':
            if (strncmp(&token[3], "wer", 3))
                break;
            switch (token[6]) {
            case '\0':
                return RBH_TOKEN_PREDICATE;
            case 'X':
                if (strcmp(&token[7], "Y") == 0)
                    return RBH_TOKEN_PREDICATE;
                break;
            }
            break;
        case 'o':
            switch (token[3]) {
            case 'g':
                if (strcmp(&token[4], "roup") == 0)
                    return RBH_TOKEN_PREDICATE;
                break;
            case 'u':
                if (strcmp(&token[4], "ser") == 0)
                    return RBH_TOKEN_PREDICATE;
                break;
            }
            break;
        }
        break;
    case 'p':
        switch (token[2]) {
        case 'a':
            if (strcmp(&token[3], "th") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'e':
            if (strcmp(&token[3], "rm") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'r':
        if (token[2] != 'e')
            break;

        switch (token[3]) {
        case 'a':
            if (strcmp(&token[4], "dable") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'g':
            if (strcmp(&token[4], "ex") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 's':
        switch (token[2]) {
        case 'a':
            if (strcmp(&token[3], "mefile") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'i':
            if (strcmp(&token[3], "ze") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 't':
        switch (token[2]) {
        case 'r':
            if (strcmp(&token[3], "ue") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'y':
            if (strcmp(&token[3], "pe") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'u':
        switch (token[2]) {
        case 'i':
            if (strcmp(&token[3], "d") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 's':
            if (token[3] != 'e')
                break;
            switch (token[4]) {
            case 'd':
                if (token[5] == '\0')
                    return RBH_TOKEN_PREDICATE;
                break;
            case 'r':
                if (token[5] == '\0')
                    return RBH_TOKEN_PREDICATE;
                break;
            }
            break;
        }
        break;
    case 'w':
        switch (token[2]) {
        case 'h':
            if (strcmp(&token[3], "olename") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 'r':
            if (strcmp(&token[3], "iteable") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    case 'x':
        switch (token[2]) {
        case 'a':
            if (strcmp(&token[3], "ttr") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        case 't':
            if (strcmp(&token[3], "ype") == 0)
                return RBH_TOKEN_PREDICATE;
            break;
        }
        break;
    }

    return RBH_TOKEN_UNKNOWN;
}
