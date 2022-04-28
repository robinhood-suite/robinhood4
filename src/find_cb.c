/* This file is part of rbh-find
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sysexits.h>

#include "rbh-find/find_cb.h"

int
find_pre_action(struct find_context *ctx, const int index,
                const enum action action)
{
    const char *filename;

    switch (action) {
    case ACT_FLS:
    case ACT_FPRINT:
    case ACT_FPRINT0:
        if (index + 1 >= ctx->argc)
            error(EX_USAGE, 0, "missing argument to `%s'", action2str(action));

        filename = ctx->argv[index + 1];
        ctx->action_file = fopen(filename, "w");
        if (ctx->action_file == NULL)
            error(EXIT_FAILURE, errno, "fopen: %s", filename);

        return 1;
    default:
        break;
    }

    return 0;
}

int
find_exec_action(struct find_context *ctx, enum action action,
                 struct rbh_fsentry *fsentry)
{
    switch (action) {
    case ACT_PRINT:
        /* XXX: glibc's printf() handles printf("%s", NULL) pretty well, but
         *      I do not think this is part of any standard.
         */
        printf("%s\n", fsentry_path(fsentry));
        break;
    case ACT_PRINT0:
        printf("%s%c", fsentry_path(fsentry), '\0');
        break;
    case ACT_FLS:
        fsentry_print_ls_dils(ctx->action_file, fsentry);
        break;
    case ACT_FPRINT:
        fprintf(ctx->action_file, "%s\n", fsentry_path(fsentry));
        break;
    case ACT_FPRINT0:
        fprintf(ctx->action_file, "%s%c", fsentry_path(fsentry), '\0');
        break;
    case ACT_LS:
        fsentry_print_ls_dils(stdout, fsentry);
        break;
    case ACT_COUNT:
        return 1;
        break;
    case ACT_QUIT:
        exit(0);
        break;
    default:
        error(EXIT_FAILURE, ENOSYS, "%s", action2str(action));
        break;
    }

    return 0;
}

void
find_post_action(struct find_context *ctx, const int index,
                 const enum action action, const size_t count)
{
    const char *filename;

    switch (action) {
    case ACT_COUNT:
        printf("%lu matching entries\n", count);
        break;
    case ACT_FLS:
    case ACT_FPRINT:
    case ACT_FPRINT0:
        filename = ctx->argv[index];
        if (fclose(ctx->action_file))
            error(EXIT_FAILURE, errno, "fclose: %s", filename);
        break;
    default:
        break;
    }
}

struct rbh_filter *
find_parse_predicate(struct find_context *ctx, int *arg_idx)
{
    struct rbh_filter *filter;
    enum predicate predicate;
    int i = *arg_idx;

    predicate = str2predicate(ctx->argv[i]);

    if (i + 1 >= ctx->argc)
        error(EX_USAGE, 0, "missing argument to `%s'", ctx->argv[i]);

    /* In the following block, functions should call error() themselves rather
     * than returning.
     *
     * Errors are most likely fatal (not recoverable), and this allows for
     * precise and meaningful error messages.
     */
    switch (predicate) {
    case PRED_AMIN:
    case PRED_MMIN:
    case PRED_CMIN:
        filter = xmin2filter(predicate, ctx->argv[++i]);
        break;
    case PRED_ATIME:
    case PRED_MTIME:
    case PRED_CTIME:
        filter = xtime2filter(predicate, ctx->argv[++i]);
        break;
    case PRED_NAME:
        filter = shell_regex2filter(predicate, ctx->argv[++i], 0);
        break;
    case PRED_INAME:
        filter = shell_regex2filter(predicate, ctx->argv[++i],
                                    RBH_RO_CASE_INSENSITIVE);
        break;
    case PRED_TYPE:
        filter = filetype2filter(ctx->argv[++i]);
        break;
    case PRED_SIZE:
        filter = filesize2filter(ctx->argv[++i]);
        break;
    case PRED_PERM:
        filter = mode2filter(ctx->argv[++i]);
        break;
    case PRED_XATTR:
        filter = xattr2filter(ctx->argv[++i]);
        break;
    default:
        error(EXIT_FAILURE, ENOSYS, "%s", ctx->argv[i]);
        /* clang: -Wsometimes-unitialized: `filter` */
        __builtin_unreachable();
    }
    assert(filter != NULL);

    *arg_idx = i;
    return filter;
}
