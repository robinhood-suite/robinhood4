/* This file is part of rbh-find
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>

#include "rbh-find/find_cb.h"

static void
open_action_file(struct find_context *ctx, const char *filename)
{
    ctx->action_file = fopen(filename, "w");
    if (ctx->action_file == NULL)
        error(EXIT_FAILURE, errno, "fopen: %s", filename);
}


static bool
validate_format_string(const char *format_string)
{
    (void) format_string;

    return true;
}

static int
parse_exec_command(struct find_context *ctx, const int index)
{
    size_t count = 0;
    int i = index;

    while (i < ctx->argc && strcmp(ctx->argv[i], ";")) {
        count++;
        i++;
    }
    // XXX test if this works
    if (i == ctx->argc)
        error(EX_USAGE, 0, "missing ';' at the end of -exec command");

    // +1 for NULL at the end
    ctx->exec_command = calloc(count + 1, sizeof(*ctx->exec_command));
    if (!ctx->exec_command)
        error(EXIT_FAILURE, errno, "parse_exec_command");

    for (size_t j = 0; j < count; j++)
        ctx->exec_command[j] = ctx->argv[index + j];

    return count;
}

int
find_pre_action(struct find_context *ctx, const int index,
                const enum action action)
{
    switch (action) {
    case ACT_EXEC:
        if (index + 2 >= ctx->argc) // at least two args: -exec cmd ;
            error(EX_USAGE, 0, "missing argument to `%s'", action2str(action));

        return 1 + parse_exec_command(ctx, index + 1); // consume -exec
    case ACT_FLS:
    case ACT_FPRINT:
    case ACT_FPRINT0:
        if (index + 1 >= ctx->argc)
            error(EX_USAGE, 0, "missing argument to `%s'", action2str(action));

        open_action_file(ctx, ctx->argv[index + 1]);

        return 1;
    case ACT_FPRINTF:
        if (index + 2 >= ctx->argc)
            error(EX_USAGE, 0, "missing argument to `%s'", action2str(action));

        open_action_file(ctx, ctx->argv[index + 1]);

        if (!validate_format_string(ctx->argv[index + 2]))
            error(EX_USAGE, 0, "missing format string to `%s'",
                  action2str(action));

        ctx->format_string = ctx->argv[index + 2];

        return 1;
    case ACT_PRINTF:
        if (index + 1 >= ctx->argc)
            error(EX_USAGE, 0, "missing argument to `%s'", action2str(action));

        if (!validate_format_string(ctx->argv[index + 1]))
            error(EX_USAGE, 0, "missing format string to `%s'",
                  action2str(action));

        ctx->format_string = ctx->argv[index + 1];

        return 1;
    default:
        break;
    }

    return 0;
}

static int
wait_process(pid_t child)
{
    int rc;

    rc = waitpid(child, NULL, 0);
    if (rc == -1)
        perror("waitpid");

    return 0;
}

static const char *
fsentry_relative_path(const struct rbh_fsentry *fsentry)
{
    const char *path = fsentry_path(fsentry);

    if (path[0] == '/' && path[1] == '\0')
        return ".";
    else
        return &path[1];
}

static int
exec_command(struct find_context *ctx, struct rbh_fsentry *fsentry)
{
    const char **cmd;
    pid_t child;
    int i = 0;

    while (ctx->exec_command[i++]);
    --i;

    cmd = calloc(i + 1, sizeof(*cmd));
    if (!cmd)
        return -1;

    for (int j = 0; j < i; j++) {
        if (!strcmp(ctx->exec_command[j], "{}"))
            cmd[j] = fsentry_relative_path(fsentry);
        else
            cmd[j] = ctx->exec_command[j];
    }

    child = fork();

    switch (child) {
    case -1:
        return -1;
    case 0:
        execvp(cmd[0], (char * const *)cmd);
        // This is exiting the child process, not the main rbh-find process.
        error(EXIT_FAILURE, errno, "failed to execute '%s'", cmd[0]);
        __builtin_unreachable();
    default:
        return wait_process(child);
    }
}

int
find_exec_action(struct find_context *ctx, enum action action,
                 struct rbh_fsentry *fsentry)
{
    switch (action) {
    case ACT_DELETE:
        return unlink(&fsentry_path(fsentry)[1]); // remove first slash
    case ACT_EXEC:
        return exec_command(ctx, fsentry);
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
    case ACT_FPRINTF:
        fsentry_printf_format(ctx->action_file, fsentry, ctx->format_string);
        break;
    case ACT_PRINTF:
        fsentry_printf_format(stdout, fsentry, ctx->format_string);
        break;
    case ACT_COUNT:
        return 1;
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
    case ACT_EXEC:
        free(ctx->exec_command);
        break;
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
    case PRED_BMIN:
    case PRED_MMIN:
    case PRED_CMIN:
        filter = xmin2filter(predicate, ctx->argv[++i]);
        break;
    case PRED_ATIME:
    case PRED_BTIME:
    case PRED_MTIME:
    case PRED_CTIME:
        filter = xtime2filter(predicate, ctx->argv[++i]);
        break;
    case PRED_PATH:
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

enum command_line_token
find_predicate_or_action(const char *string)
{
    switch (string[1]) {
    case 'a':
    case 'b':
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
