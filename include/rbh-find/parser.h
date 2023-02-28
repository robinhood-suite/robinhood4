/* This file is part of rbh-find
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_PARSER_H
#define RBH_FIND_PARSER_H

enum command_line_token {
    CLT_URI,
    CLT_AND,
    CLT_OR,
    CLT_NOT,
    CLT_PARENTHESIS_OPEN,
    CLT_PARENTHESIS_CLOSE,
    CLT_PREDICATE,
    CLT_ACTION,
    CLT_SORT,
    CLT_RSORT,
};

enum predicate {
    PRED_AMIN,
    PRED_ANEWER,
    PRED_ATIME,
    PRED_BMIN,
    PRED_BNEWER,
    PRED_BTIME,
    PRED_CMIN,
    PRED_CNEWER,
    PRED_CONTEXT,
    PRED_CTIME,
    PRED_EMPTY,
    PRED_EXECUTABLE,
    PRED_FALSE,
    PRED_FSTYPE,
    PRED_GID,
    PRED_GROUP,
    PRED_ILNAME,
    PRED_INAME,
    PRED_INUM,
    PRED_IPATH,
    PRED_IREGEX,
    PRED_IWHOLENAME,
    PRED_LINKS,
    PRED_LNAME,
    PRED_MMIN,
    PRED_MTIME,
    PRED_NAME,
    PRED_NEWER,
    PRED_NEWERXY,
    PRED_NOGROUP,
    PRED_NOUSER,
    PRED_PATH,
    PRED_PERM,
    PRED_READABLE,
    PRED_REGEX,
    PRED_SAMEFILE,
    PRED_SIZE,
    PRED_TRUE,
    PRED_TYPE,
    PRED_UID,
    PRED_USED,
    PRED_USER,
    PRED_WHOLENAME,
    PRED_WRITEABLE,
    PRED_XATTR,
    PRED_XTYPE,

    PRED_LAST,
};

/**
 * str2predicate - convert a string to a predicate
 *
 * @param string    a string representing a valid predicate
 *
 * @return          the predicate that \p string represents
 *
 * This function will exit if \p string is not a valid predicate
 */
enum predicate
str2predicate(const char *string);

/**
 * predicate2str - convert a predicate to a string
 *
 * @param predicate a predicate
 *
 * @return          the string that represents \p predicate
 */
const char *
predicate2str(enum predicate);

enum action {
    ACT_COUNT,
    ACT_DELETE,
    ACT_EXEC,
    ACT_EXECDIR,
    ACT_FLS,
    ACT_FPRINT,
    ACT_FPRINT0,
    ACT_FPRINTF,
    ACT_LS,
    ACT_OK,
    ACT_OKDIR,
    ACT_PRINT,
    ACT_PRINT0,
    ACT_PRINTF,
    ACT_PRUNE,
    ACT_QUIT,
};

/**
 * str2action - convert a string to an action
 *
 * @param string    a string representing a valid action
 *
 * @return          the action \p string represents
 *
 * This function will exit if \p string is not a valid predicate
 */
enum action
str2action(const char *string);

/**
 * action2str - convert an action to a string
 *
 * @param action    an action
 *
 * @return          the string that represents \p action
 */
const char *
action2str(enum action action);

#endif
