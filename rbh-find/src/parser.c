/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <error.h>
#include <stdbool.h>
#include <string.h>
#include <sysexits.h>

#include "actions.h"
#include "core.h"
#include "filters.h"
#include "parser.h"

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

void
find_parse_callback(struct filters_context *ctx, int *arg_idx,
                    const struct rbh_filter *filter,
                    struct rbh_filter_options *options,
                    enum command_line_token token, void *param)
{
    struct find_context *find_ctx = (struct find_context *)param;
    bool ascending = true;

    switch (token) {
    case CLT_RSORT:
        /* Set a descending sort option */
        ascending = false;
        __attribute__((fallthrough));
    case CLT_SORT:
        /* Build an options filter from the sort option and its arguments */
        if (*arg_idx + 1 >= ctx->argc)
            error(EX_USAGE, 0, "missing argument to '%s'", ctx->argv[*arg_idx]);

        sort_options_append(options, str2field(ctx->argv[++(*arg_idx)]),
                            ascending);
        break;
    case CLT_ACTION:
        find_ctx->action_done = true;
        find(find_ctx, str2action(ctx->argv[*arg_idx]), arg_idx,
             filter, options);
        break;
    default:
        break;
    }
}
