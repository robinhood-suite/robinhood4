/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef RBH_FIND_PARSER_H
#define RBH_FIND_PARSER_H

enum command_line_token {
    CLT_URI,
    CLT_AND,
    CLT_OR,
    CLT_NOT,
    CLT_PARENTHESE_OPEN,
    CLT_PARENTHESE_CLOSE,
    CLT_PREDICATE,
    CLT_ACTION,
};

/**
 * str2command_line_token - command line token classifier
 *
 * @param string    the string to classify
 *
 * @return          the command_line_token that most like represents \p string
 *
 * \p string does not need to be a valid token
 */
enum command_line_token
str2command_line_token(const char *string);

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

#endif
