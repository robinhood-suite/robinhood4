/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <error.h>
#include <string.h>
#include <sysexits.h>

#include "robinhood/backends/posix.h"
#include "parser.h"

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
    case 'b':
        switch (string[2]) {
        case 'l':
            if (strcmp(&string[3], "ocks") == 0)
                return PRED_BLOCKS;
            break;
        case 'm':
            if (strcmp(&string[3], "in") == 0)
                return PRED_BMIN;
            break;
        case 't':
            if (strcmp(&string[3], "ime") == 0)
                return PRED_BTIME;
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
        break;
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
        switch (string[2]) {
        case 'a':
            if (strcmp(&string[3], "ttr") == 0)
                return PRED_XATTR;
            break;
        case 't':
            if (strcmp(&string[3], "ype") == 0)
                return PRED_XTYPE;
            break;
        }
        break;
    }
    error(EX_USAGE, 0, "unknown predicate: `%s'", string);
    __builtin_unreachable();
}

static const char *__predicate2str[] = {
    [PRED_AMIN]         = "amin",
    [PRED_ANEWER]       = "anewer",
    [PRED_ATIME]        = "atime",
    [PRED_BLOCKS]       = "blocks",
    [PRED_BMIN]         = "bmin",
    [PRED_BNEWER]       = "bnewer",
    [PRED_BTIME]        = "btime",
    [PRED_CMIN]         = "cmin",
    [PRED_CNEWER]       = "cnewer",
    [PRED_CONTEXT]      = "context",
    [PRED_CTIME]        = "ctime",
    [PRED_EMPTY]        = "empty",
    [PRED_EXECUTABLE]   = "executable",
    [PRED_FALSE]        = "false",
    [PRED_FSTYPE]       = "fstype",
    [PRED_GID]          = "gid",
    [PRED_GROUP]        = "group",
    [PRED_ILNAME]       = "ilname",
    [PRED_INAME]        = "iname",
    [PRED_INUM]         = "inum",
    [PRED_IPATH]        = "ipath",
    [PRED_IREGEX]       = "iregex",
    [PRED_IWHOLENAME]   = "iwholename",
    [PRED_LINKS]        = "links",
    [PRED_LNAME]        = "lname",
    [PRED_MMIN]         = "mmin",
    [PRED_MTIME]        = "mtime",
    [PRED_NAME]         = "name",
    [PRED_NEWER]        = "newer",
    [PRED_NEWERXY]      = "newerXY",
    [PRED_NOGROUP]      = "nogroup",
    [PRED_NOUSER]       = "nouser",
    [PRED_PATH]         = "path",
    [PRED_PERM]         = "perm",
    [PRED_READABLE]     = "readable",
    [PRED_REGEX]        = "regex",
    [PRED_SAMEFILE]     = "samefile",
    [PRED_SIZE]         = "size",
    [PRED_TRUE]         = "true",
    [PRED_TYPE]         = "type",
    [PRED_UID]          = "uid",
    [PRED_USED]         = "used",
    [PRED_USER]         = "user",
    [PRED_WHOLENAME]    = "wholename",
    [PRED_WRITEABLE]    = "writeable",
    [PRED_XATTR]        = "xattr",
    [PRED_XTYPE]        = "xtype",
};

const char *
predicate2str(enum predicate predicate)
{
    return __predicate2str[predicate];
}
