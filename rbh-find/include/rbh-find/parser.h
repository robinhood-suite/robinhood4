/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
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
