/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <grp.h>
#include <inttypes.h>
#include <linux/limits.h>
#include <libgen.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <robinhood/statx.h>
#include <robinhood/utils.h>

#include "core.h"
#include "actions.h"

static struct tm now;

static void __attribute__((constructor))
now_init(void)
{
    time_t tmp;

    tmp = time(NULL);
    if (localtime_r(&tmp, &now) == NULL)
        error(EXIT_FAILURE, errno, "localtime_r");
}

/* Timestamp string is: "Jan 31 12:00" or "Jan 31  2000" */
static void
timestamp_print_ls_dils(FILE *file, int64_t timestamp)
{
    static_assert(sizeof(time_t) >= sizeof(int64_t), "");
    char buffer[sizeof("Jan 31 12:00")];
    time_t duration = timestamp;
    struct tm *datetime;

    datetime = localtime(&duration);
    if (datetime == NULL)
        error(EXIT_FAILURE, errno, "localtime");

    strftime(buffer, sizeof(buffer),
             datetime->tm_year < now.tm_year ? "%b %e  %G" : "%b %e %H:%M",
             datetime);

    fprintf(file, "%s", buffer);
}

/**
 * Convert a mode to a type
 *
 * @param mode  a mode of a fsentry
 *
 * @return      a character representing the type
 */
static char
mode2type(mode_t mode)
{
    if (S_ISREG(mode))
        return '-';
    if (S_ISDIR(mode))
        return 'd';
    if (S_ISLNK(mode))
        return 'l';
    if (S_ISCHR(mode))
        return 'c';
    if (S_ISBLK(mode))
        return 'b';
    if (S_ISFIFO(mode))
        return 'p';
    if (S_ISSOCK(mode))
        return 's';
    error(EXIT_FAILURE, EINVAL, "non existant fsentry's type");
     __builtin_unreachable();
}

/* The special bits */
static const mode_t SPECIAL_BITS[] = {
    0, 0, S_ISUID,
    0, 0, S_ISGID,
    0, 0, S_ISVTX
};

/* The 9 mode bits to test */
static const mode_t MODE_BITS[] = {
    S_IRUSR, S_IWUSR, S_IXUSR,
    S_IRGRP, S_IWGRP, S_IXGRP,
    S_IROTH, S_IWOTH, S_IXOTH
};

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(array) sizeof(array) / sizeof(array[0])
#endif

static void
mode_print_ls_dils(FILE *file, mode_t mode)
{
    static_assert(ARRAY_SIZE(MODE_BITS) == ARRAY_SIZE(SPECIAL_BITS), "");
    for (size_t i = 0; i < ARRAY_SIZE(MODE_BITS); i++) {
        const char *mapping =
            mode & SPECIAL_BITS[i] ? mode & MODE_BITS[i] ? "..s..s..t"
                                                         : "..S..S..T"
                                   : mode & MODE_BITS[i] ? "rwxrwxrwx"
                                                         : "---------";
        fprintf(file, "%c", mapping[i]);
    }
}

static bool posixly_correct;

static void __attribute__((constructor))
posixly_correct_init(void)
{
    posixly_correct = secure_getenv("POSIXLY_CORRECT") != NULL;
}

static void
statx_print_ls_dils(FILE *file, const struct rbh_statx *statxbuf)
{
    static struct {
        int ino;
        int blocks;
        int nlink;
        int uid;
        int gid;
        int size;
    } length = {
        .ino = 9,
        .blocks = 6,
        .nlink = 3,
        .uid = 8,
        .gid = 8,
        .size = 8,
    };
    int rc;

    if (statxbuf == NULL) {
        /*              -rwxrwxrwx                 Jan 31 20:00 */
        fprintf(file, "%*c %*c ?????????? %*c %*c %*c %*c ????????????",
                length.ino, '?', length.blocks, '?', length.nlink, '?',
                length.uid, '?', length.gid, '?', length.size, '?');
        return;
    }

    if (statxbuf->stx_mask & RBH_STATX_INO) {
        rc = fprintf(file, "%*" PRIu64, length.ino, statxbuf->stx_ino);
        length.ino = MAX(length.ino, rc);
    } else {
        fprintf(file, "%*c", length.ino, '?');
    }

    if (statxbuf->stx_mask & RBH_STATX_BLOCKS) {
        uint64_t blocks = posixly_correct ? statxbuf->stx_blocks
                                          : statxbuf->stx_blocks / 2;

        /* The `-1` makes up for the space before the string */
        rc = fprintf(file, " %*ld", length.blocks, blocks) - 1;
        length.blocks = MAX(length.blocks, rc);
    } else {
        fprintf(file, " %*c", length.blocks, '?');
    }

    fprintf(file, " %c",
           statxbuf->stx_mask & RBH_STATX_TYPE ? mode2type(statxbuf->stx_mode)
                                           : '?');

    if (statxbuf->stx_mask & RBH_STATX_MODE)
        mode_print_ls_dils(file, statxbuf->stx_mode);
    else
        /*      rwxrwxrwx */
        fprintf(file, "?????????");

    if (statxbuf->stx_mask & RBH_STATX_NLINK) {
        rc = fprintf(file, " %*d", length.nlink, statxbuf->stx_nlink) - 1;
        length.nlink = MAX(length.nlink, rc);
    } else {
        fprintf(file, " %*c", length.nlink, '?');
    }

    if (statxbuf->stx_mask & RBH_STATX_UID) {
        const struct passwd *uid = getpwuid(statxbuf->stx_uid);

        if (uid)
            rc = fprintf(file, " %-*s", length.uid, uid->pw_name) - 1;
        else
            rc = fprintf(file, " %*d", length.uid, statxbuf->stx_uid) - 1;

        length.uid = MAX(length.uid, rc);
    } else {
        fprintf(file, " %*c", length.uid, '?');
    }

    if (statxbuf->stx_mask & RBH_STATX_GID) {
        const struct group *gid = getgrgid(statxbuf->stx_gid);

        if (gid)
            rc = fprintf(file, " %-*s", length.gid, gid->gr_name) - 1;
        else
            rc = fprintf(file, " %*d", length.gid, statxbuf->stx_gid) - 1;

        length.gid = MAX(length.gid, rc);
    } else {
        fprintf(file, " %*c", length.gid, '?');
    }

    if (statxbuf->stx_mask & RBH_STATX_SIZE) {
        rc = fprintf(file, " %*" PRIu64, length.size, statxbuf->stx_size) - 1;
        length.size = MAX(length.size, rc);
    } else {
        fprintf(file, " %*c", length.size, '?');
    }

    fprintf(file, " ");
    if (statxbuf->stx_mask & RBH_STATX_MTIME_SEC)
        timestamp_print_ls_dils(file, statxbuf->stx_mtime.tv_sec);
    else
        /*      Jan 31 20:00 */
        fprintf(file, "????????????");
}

void
fsentry_print_ls_dils(FILE *file, const struct rbh_fsentry *fsentry)
{
    statx_print_ls_dils(file,
                        fsentry->mask & RBH_FP_STATX ? fsentry->statx : NULL);

    fprintf(file, " %s", fsentry_path(fsentry));

    if (fsentry->mask & RBH_FP_SYMLINK)
        fprintf(file, " -> %s", fsentry->symlink);

    fprintf(file, "\n");
}

#define MAX_OUTPUT_SIZE (PATH_MAX + 256)

static int
fsentry_print_escape(char *output, int max_length, const char *escape)
{
    assert(escape != NULL);
    assert(*escape != '\0');

    (void) max_length;

    /* For now, consider the escape to be a single character */
    switch (*escape) {
    case 'n':
        *output = '\n';
        return 1;
    default:
        error(EXIT_FAILURE, ENOTSUP, "format escape not supported");
    }

    __builtin_unreachable();
}

static int
fsentry_print_regular_char(char *output, int max_length,
                           const char *format_string)
{
    int sublength = 0;

    assert(format_string != NULL);
    assert(*format_string != '\0');

    while (format_string[sublength] != '%' &&
           format_string[sublength] != '\\' &&
           format_string[sublength] != 0 &&
           sublength < max_length) {
        output[sublength] = format_string[sublength];
        sublength++;
    }

    return sublength;
}

void
fsentry_printf_format(struct find_context *ctx, size_t backend_index,
                      const struct rbh_fsentry *fsentry)
{
    const char *format_string = ctx->format_string;
    const char *backend = ctx->uris[backend_index];
    size_t length = strlen(format_string);
    int max_length = MAX_OUTPUT_SIZE;
    FILE *file = ctx->action_file;
    char output[MAX_OUTPUT_SIZE];
    size_t output_length = 0;

    for (size_t i = 0; i < length; i++) {
        int tmp_length = 0;

        switch (format_string[i]) {
        case '%':
            for (int j = 0; j < ctx->f_ctx.info_pe_count; ++j) {
                const struct rbh_pe_common_operations *common_ops =
                    get_common_operations(&ctx->f_ctx.info_pe[j]);

                tmp_length = rbh_pe_common_ops_fill_entry_info(
                    common_ops, &output[output_length], max_length, fsentry,
                    format_string + i + 1, backend
                );

                if (tmp_length > 0)
                    break;
            }
            /* Go over the directive that was just printed */
            i++;
            break;
        case '\\':
            tmp_length = fsentry_print_escape(&output[output_length],
                                              max_length,
                                              format_string + i + 1);
            /* Go over the escape that was just printed */
            i++;
            break;
        default:
            tmp_length = fsentry_print_regular_char(&output[output_length],
                                                    max_length,
                                                    format_string + i);
            i += tmp_length - 1;
        }

        output_length += tmp_length;
        max_length -= tmp_length;

        if (tmp_length < 0) {
            error(EXIT_FAILURE, EINVAL, "failed to write to buffer for printf");
        } else if (max_length <= 0) {
            /* No more data can fit in the output array */
            output_length = MAX_OUTPUT_SIZE - 1;
            break;
        }
    }

    output[output_length] = 0;
    fprintf(file, "%s", output);
}

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
        {
            int rc;

            for (int i = 0; i < ctx->f_ctx.info_pe_count; ++i) {
                const struct rbh_pe_common_operations *common_ops =
                    get_common_operations(&ctx->f_ctx.info_pe[i]);

                rc = rbh_pe_common_ops_delete_entry(common_ops, fsentry);
                if (rc == 0)
                    return rc;
            }

            return rc;
        }
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
        fsentry_printf_format(ctx, backend_index, fsentry);
        break;
    case ACT_PRINTF:
        ctx->action_file = stdout;
        fsentry_printf_format(ctx, backend_index, fsentry);
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
