/* This file is part of rbh-find
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
        if (strcmp(&string[2], "type") == 0)
            return PRED_XTYPE;
        break;
    }
    error(EX_USAGE, 0, "unknown predicate: `%s'", string);
    __builtin_unreachable();
}

static const char *__predicate2str[] = {
    [PRED_AMIN]         = "amin",
    [PRED_ANEWER]       = "anewer",
    [PRED_ATIME]        = "atime",
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
    [PRED_XTYPE]        = "xtype",
};

const char *
predicate2str(enum predicate predicate)
{
    return __predicate2str[predicate];
}

enum action
str2action(const char *string)
{
    assert(string[0] == '-');

    switch (string[1]) {
    case 'c':
        if (strcmp(&string[2], "ount") == 0)
            return ACT_COUNT;
        break;
    case 'd':
        if (strcmp(&string[2], "elete") == 0)
            return ACT_DELETE;
        break;
    case 'e':
        if (strncmp(&string[2], "xec", 3))
            break;

        switch (string[5]) {
        case '\0':
            return ACT_EXEC;
        case 'd':
            if (strcmp(&string[6], "ir") == 0)
                return ACT_EXECDIR;
            break;
        }
        break;
    case 'f':
        switch (string[2]) {
        case 'l':
            if (strcmp(&string[3], "s") == 0)
                return ACT_FLS;
            break;
        case 'p':
            if (strncmp(&string[3], "rint", 4))
                break;

            switch (string[7]) {
            case '\0':
                return ACT_FPRINT;
            case '0':
                if (string[8] == '\0')
                    return ACT_FPRINT0;
                break;
            case 'f':
                if (string[8] == '\0')
                    return ACT_FPRINTF;
                break;
            }
            break;
        }
        break;
    case 'l':
        if (strcmp(&string[2], "s") == 0)
            return ACT_LS;
        break;
    case 'o':
        if (string[2] != 'k')
            break;

        switch (string[3]) {
        case '\0':
            return ACT_OK;
        case 'd':
            if (strcmp(&string[4], "ir") == 0)
                return ACT_OKDIR;
            break;
        }
        break;
    case 'p':
        if (string[2] != 'r')
            break;

        switch (string[3]) {
        case 'i':
            if (strncmp(&string[4], "nt", 2))
                break;

            switch (string[6]) {
            case '\0':
                return ACT_PRINT;
            case '0':
                if (string[7] == '\0')
                    return ACT_PRINT0;
                break;
            case 'f':
                if (string[7] == '\0')
                    return ACT_PRINTF;
                break;
            }
            break;
        case 'u':
            if (strcmp(&string[4], "ne") == 0)
                return ACT_PRUNE;
            break;
        }
        break;
    case 'q':
        if (strcmp(&string[2], "uit") == 0)
            return ACT_QUIT;
        break;
    }
    error(EX_USAGE, 0, "unknown predicate `%s'", string);
    __builtin_unreachable();
}

static const char *__action2str[] = {
    [ACT_COUNT]     = "-count",
    [ACT_DELETE]    = "-delete",
    [ACT_EXEC]      = "-exec",
    [ACT_EXECDIR]   = "-execdir",
    [ACT_FLS]       = "-fls",
    [ACT_FPRINT]    = "-fprint",
    [ACT_FPRINT0]   = "-fprint0",
    [ACT_FPRINTF]   = "-fprintf",
    [ACT_LS]        = "-ls",
    [ACT_OK]        = "-ok",
    [ACT_OKDIR]     = "-okdir",
    [ACT_PRINT]     = "-print",
    [ACT_PRINT0]    = "-print0",
    [ACT_PRINTF]    = "-printf",
    [ACT_PRUNE]     = "-prune",
    [ACT_QUIT]      = "-quit",
};

const char *
action2str(enum action action)
{
    return __action2str[action];
}
