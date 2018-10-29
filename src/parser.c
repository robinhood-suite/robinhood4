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

#include <assert.h>
#include <error.h>
#include <string.h>
#include <sysexits.h>

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

enum predicate
str2predicate(const char *string)
{
    assert(string[0] == '-');

    switch (string[1]) {
    case 'a':
        switch (string[2]) {
        case 'm':
            if (strcmp(&string[3], "in") == 0)
                return PRED_AMIN;
            break;
        case 'n':
            if (strcmp(&string[3], "ewer") == 0)
                return PRED_ANEWER;
            break;
        case 't':
            if (strcmp(&string[3], "ime") == 0)
                return PRED_ATIME;
            break;
        }
        break;
    case 'c':
        switch (string[2]) {
        case 'm':
            if (strcmp(&string[3], "in") == 0)
                return PRED_CMIN;
            break;
        case 'n':
            if (strcmp(&string[3], "ewer") == 0)
                return PRED_CNEWER;
            break;
        case 'o':
            if (strcmp(&string[3], "ntext") == 0)
                return PRED_CONTEXT;
            break;
        case 't':
            if (strcmp(&string[3], "ime") == 0)
                return PRED_CTIME;
            break;
        }
        break;
    case 'e':
        switch (string[2]) {
        case 'm':
            if (strcmp(&string[3], "pty") == 0)
                return PRED_EMPTY;
            break;
        case 'x':
            if (strcmp(&string[3], "ecutable") == 0)
                return PRED_EXECUTABLE;
            break;
        }
        break;
    case 'f':
        switch (string[2]) {
        case 'a':
            if (strcmp(&string[3], "lse") == 0)
                return PRED_FALSE;
            break;
        case 's':
            if (strcmp(&string[3], "type") == 0)
                return PRED_FSTYPE;
            break;
        }
        break;
    case 'g':
        switch (string[2]) {
        case 'i':
            if (strcmp(&string[3], "d") == 0)
                return PRED_GID;
            break;
        case 'r':
            if (strcmp(&string[3], "oup") == 0)
                return PRED_GROUP;
            break;
        }
        break;
    case 'i':
        switch (string[2]) {
        case 'l':
            if (strcmp(&string[3], "name") == 0)
                return PRED_ILNAME;
            break;
        case 'n':
            switch (string[3]) {
            case 'a':
                if (strcmp(&string[4], "me") == 0)
                    return PRED_INAME;
                break;
            case 'u':
                if (strcmp(&string[4], "m") == 0)
                    return PRED_INUM;
                break;
            }
            break;
        case 'p':
            if (strcmp(&string[3], "ath") == 0)
                return PRED_IPATH;
            break;
        case 'r':
            if (strcmp(&string[3], "egex") == 0)
                return PRED_IREGEX;
            break;
        case 'w':
            if (strcmp(&string[3], "holename") == 0)
                return PRED_IWHOLENAME;
            break;
        }
        break;
    case 'l':
        switch (string[2]) {
        case 'i':
            if (strcmp(&string[3], "nks") == 0)
                return PRED_LINKS;
            break;
        case 'n':
            if (strcmp(&string[3], "ame") == 0)
                return PRED_LNAME;
            break;
        }
        break;
    case 'm':
        switch (string[2]) {
        case 'm':
            if (strcmp(&string[3], "in") == 0)
                return PRED_MMIN;
            break;
        case 't':
            if (strcmp(&string[3], "ime") == 0)
                return PRED_MTIME;
            break;
        }
        break;
    case 'n':
        switch (string[2]) {
        case 'a':
            if (strcmp(&string[3], "me") == 0)
                return PRED_NAME;
            break;
        case 'e':
            if (strncmp(&string[3], "wer", 3))
                break;
            switch (string[6]) {
            case '\0':
                return PRED_NEWER;
            case 'X':
                if (strcmp(&string[7], "Y") == 0)
                    return PRED_NEWERXY;
                break;
            }
            break;
        case 'o':
            switch (string[3]) {
            case 'g':
                if (strcmp(&string[4], "roup") == 0)
                    return PRED_NOGROUP;
                break;
            case 'u':
                if (strcmp(&string[4], "ser") == 0)
                    return PRED_NOUSER;
                break;
            }
            break;
        }
    case 'p':
        switch (string[2]) {
        case 'a':
            if (strcmp(&string[3], "th") == 0)
                return PRED_PATH;
            break;
        case 'e':
            if (strcmp(&string[3], "rm") == 0)
                return PRED_PERM;
            break;
        }
        break;
    case 'r':
        if (string[2] != 'e')
            break;

        switch (string[3]) {
        case 'a':
            if (strcmp(&string[4], "dable") == 0)
                return PRED_READABLE;
            break;
        case 'g':
            if (strcmp(&string[4], "ex") == 0)
                return PRED_REGEX;
            break;
        }
        break;
    case 's':
        switch (string[2]) {
        case 'a':
            if (strcmp(&string[3], "mefile") == 0)
                return PRED_SAMEFILE;
            break;
        case 'i':
            if (strcmp(&string[3], "ze") == 0)
                return PRED_SIZE;
            break;
        }
        break;
    case 't':
        switch (string[2]) {
        case 'r':
            if (strcmp(&string[3], "ue") == 0)
                return PRED_TRUE;
            break;
        case 'y':
            if (strcmp(&string[3], "pe") == 0)
                return PRED_TYPE;
            break;
        }
        break;
    case 'u':
        switch (string[2]) {
        case 'i':
            if (strcmp(&string[3], "d") == 0)
                return PRED_UID;
            break;
        case 's':
            if (string[3] != 'e')
                break;
            switch (string[4]) {
            case 'd':
                if (string[5] == '\0')
                    return PRED_USED;
                break;
            case 'r':
                if (string[5] == '\0')
                    return PRED_USER;
                break;
            }
            break;
        }
        break;
    case 'w':
        switch (string[2]) {
        case 'h':
            if (strcmp(&string[3], "olename") == 0)
                return PRED_WHOLENAME;
            break;
        case 'r':
            if (strcmp(&string[3], "iteable") == 0)
                return PRED_WRITEABLE;
            break;
        }
        break;
    case 'x':
        if (strcmp(&string[2], "type") == 0)
            return PRED_XTYPE;
        break;
    }
    error(EX_USAGE, 0, "unknown predicate: `%s'", string);
    __builtin_unreachable();
}
