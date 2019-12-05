/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "parser.h"

#include <string.h>

static enum command_line_token
predicate_or_action(const char *string)
{
    switch (string[1]) {
    case 'a':
    case 'g':
    case 'i':
    case 'm':
    case 'n':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'w':
    case 'x':
        return CLT_PREDICATE;
    case 'c':
        if (string[2] == '\0' || string[3] != 'u')
            return CLT_PREDICATE;
        return CLT_ACTION;
    case 'e':
        if (strncmp(&string[2], "xec", 3))
            return CLT_PREDICATE;

        switch (string[5]) {
        case 'u':
            return CLT_PREDICATE;
        }
        return CLT_ACTION;
    case 'f':
        switch (string[2]) {
        case 'a':
        case 's':
            return CLT_PREDICATE;
        }
        return CLT_ACTION;
    case 'l':
        switch (string[2]) {
        case 'i':
        case 'n':
            return CLT_PREDICATE;
        }
        return CLT_ACTION;
    case 'p':
        if (string[2] != 'r')
            return CLT_PREDICATE;
        return CLT_ACTION;
    }
    return CLT_ACTION;
}

enum command_line_token
str2command_line_token(const char *string)
{
    switch (string[0]) {
    case '(':
        if (string[1] == '\0')
            return CLT_PARENTHESIS_OPEN;
        break;
    case ')':
        if (string[1] == '\0')
            return CLT_PARENTHESIS_CLOSE;
        break;
    case '!':
        if (string[1] == '\0')
            return CLT_NOT;
        break;
    case '-':
        switch (string[1]) {
        case 'a':
            if (string[2] == '\0' || strcmp(&string[2], "nd") == 0)
                return CLT_AND;
            break;
        case 'o':
            if (string[2] == '\0' || strcmp(&string[2], "r") == 0)
                return CLT_OR;
            break;
        case 'n':
            if (strcmp(&string[2], "ot") == 0)
                return CLT_NOT;
            break;
        }
        return predicate_or_action(string);
    }
    return CLT_URI;
}
