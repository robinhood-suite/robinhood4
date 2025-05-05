/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_PARSER_H
#define RBH_FIND_PARSER_H

#include <rbh-filters/parser.h>

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

/**
 * find_parse_callback - parse an expression  that have a specific handle for
 *                       find
 *
 * @param argc          number of argument in the command line to parse
 * @param argv          the command line to parse
 * @param arg_idx       a pointer to the index of argv to start parsing at
 * @param filter        a filter (the part of the cli already parsed)
 * @param sorts         an array of filtering options
 * @param sort_count    the size of \p sorts
 * @param token         the token to parse
 * @param param         callback argument
 */
void
find_parse_callback(int argc, char **argv, int *arg_idx,
                    const struct rbh_filter *filter,
                    struct rbh_filter_sort **sorts, size_t *sort_count,
                    enum command_line_token token, void *param);

#endif
