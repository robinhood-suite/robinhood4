/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
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

static size_t
count_exec_command_args(char **exec_command)
{
    size_t size = 0;

    /* The list of arguments is NULL terminated. */
    while (exec_command[size++]);

    /* At the end of the loop, size includes the last NULL, so we remove 1 to
     * ignore it.
     */
    return size - 1;
}

static size_t
count_subtitutions(const char *arg)
{
    const char *cursor = arg;
    size_t count = 0;

    while ((cursor = strstr(cursor, "{}"))) {
        count++;
        cursor += 2;
    }

    return count;
}

static ssize_t
substituted_size(const char *arg, const char *substitution)
{
    size_t substitution_len;
    size_t arg_len;
    size_t count;

    count = count_subtitutions(arg);
    if (count == 0)
        return -1;

    arg_len = strlen(arg);
    substitution_len = strlen(substitution);

    return arg_len + count * (substitution_len - 2) + 1;
}

static char *
append_range(char *buffer, const char *start, const char *end)
{
    strncpy(buffer, start, end - start);

    return buffer + (end - start);
}

static const char *
substitute_path(const char *arg, const char *path, char *buffer)
{
    size_t path_len = strlen(path);
    const char *prev_cursor = arg;
    const char *cursor = arg;
    char *buf_iter;

    buffer[0] = '\0';
    buf_iter = buffer;

    while ((cursor = strstr(cursor, "{}"))) {
        buf_iter = append_range(buf_iter, prev_cursor, cursor);
        buf_iter = append_range(buf_iter, path, path + path_len);

        cursor += 2;
        prev_cursor = cursor;
    }

    strcpy(buf_iter, prev_cursor);

    return buffer;
}

static const char *
resolve_arg(const char *arg, struct rbh_fsentry *fsentry)
{
    const char *substitution;
    ssize_t substitute_size;
    char *buffer;

    substitution = fsentry_relative_path(fsentry);
    substitute_size = substituted_size(arg, substitution);
    if (substitute_size == -1)
        /* no substitution to do */
        return arg;

    buffer = malloc((size_t)substitute_size);
    if (!buffer)
        error(EXIT_FAILURE, errno, "malloc in substitute_path");

    return substitute_path(arg, substitution, buffer);
}

static int
exec_command(struct find_context *ctx, struct rbh_fsentry *fsentry)
{
    const char **cmd;
    pid_t child;
    int i;

    i = count_exec_command_args(ctx->exec_command);

    cmd = calloc(i + 1, sizeof(*cmd));
    if (!cmd)
        return -1;

    for (int j = 0; j < i; j++)
        cmd[j] = resolve_arg(ctx->exec_command[j], fsentry);

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
        for (int j = 0; j < i; j++) {
            /* substitute_path only allocates if a substitution is required */
            if (cmd[j] != ctx->exec_command[j])
                free((char *)cmd[j]);
        }
        free(cmd);
        return wait_process(child);
    }
}

int
find_exec_action(struct find_context *ctx,
                 size_t backend_index,
                 enum action action,
                 struct rbh_fsentry *fsentry)
{
    switch (action) {
    case ACT_DELETE:
        return unlink(fsentry_relative_path(fsentry));
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
        fsentry_printf_format(ctx->action_file, fsentry, ctx->format_string,
                              ctx->uris[backend_index],
                              ctx->print_directive);
        break;
    case ACT_PRINTF:
        fsentry_printf_format(stdout, fsentry, ctx->format_string,
                              ctx->uris[backend_index],
                              ctx->print_directive);
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

bool
predicate_needs_argument(enum predicate predicate)
{
    switch (predicate) {
    case PRED_EMPTY:
    case PRED_NOGROUP:
    case PRED_NOUSER:
        return false;
    default:
        return true;
    }

    __builtin_unreachable();
}

struct rbh_filter *
find_parse_predicate(struct find_context *ctx, int *arg_idx)
{
    struct rbh_filter *filter;
    enum predicate predicate;
    int i = *arg_idx;

    predicate = str2predicate(ctx->argv[i]);

    if (i + 1 >= ctx->argc && predicate_needs_argument(predicate))
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
    case PRED_ANEWER:
        ctx->need_prefetch = true;
        filter = newer2filter(PRED_ATIME, ctx->argv[++i]);
        break;
    case PRED_ATIME:
    case PRED_BTIME:
    case PRED_MTIME:
    case PRED_CTIME:
        filter = xtime2filter(predicate, ctx->argv[++i]);
        break;
    case PRED_CNEWER:
        ctx->need_prefetch = true;
        filter = newer2filter(PRED_CTIME, ctx->argv[++i]);
        break;
    case PRED_EMPTY:
        filter = empty2filter();
        break;
    case PRED_ILNAME:
        filter = lname2filter(predicate, ctx->argv[++i], RBH_RO_ALL);
        break;
    case PRED_INAME:
        filter = regex2filter(predicate, ctx->argv[++i], RBH_RO_ALL);
        break;
    case PRED_IREGEX:
        filter = regex2filter(PRED_PATH, ctx->argv[++i],
                              RBH_RO_CASE_INSENSITIVE);
        break;
    case PRED_BLOCKS:
    case PRED_GID:
    case PRED_LINKS:
    case PRED_UID:
        filter = number2filter(predicate, ctx->argv[++i]);
        break;
    case PRED_GROUP:
        filter = groupname2filter(ctx->argv[++i]);
        break;
    case PRED_LNAME:
        filter = lname2filter(predicate, ctx->argv[++i], RBH_RO_SHELL_PATTERN);
        break;
    case PRED_NAME:
    case PRED_PATH:
        filter = regex2filter(predicate, ctx->argv[++i], RBH_RO_SHELL_PATTERN);
        break;
    case PRED_NEWER:
        ctx->need_prefetch = true;
        filter = newer2filter(PRED_MTIME, ctx->argv[++i]);
        break;
    case PRED_NOGROUP:
        filter = nogroup2filter();
        break;
    case PRED_NOUSER:
        filter = nouser2filter();
        break;
    case PRED_PERM:
        filter = mode2filter(ctx->argv[++i]);
        break;
    case PRED_REGEX:
        filter = regex2filter(PRED_PATH, ctx->argv[++i], 0);
        break;
    case PRED_SIZE:
        filter = filesize2filter(ctx->argv[++i]);
        break;
    case PRED_TYPE:
        filter = filetype2filter(ctx->argv[++i]);
        break;
    case PRED_USER:
        filter = username2filter(ctx->argv[++i]);
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
