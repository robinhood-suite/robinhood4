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

#include <robinhood/statx.h>

#include "rbh-find/actions.h"
#include "rbh-find/utils.h"

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

static const char*
get_group_name(uint32_t gid)
{
    struct group *group;

    group = getgrgid(gid);
    if (!group)
        return NULL;
    else
        return group->gr_name;
}

static const char *
get_user_name(uint32_t uid)
{
    struct passwd *passwd;

    passwd = getpwuid(uid);
    if (!passwd)
        return NULL;
    else
        return passwd->pw_name;
}

static char
type2char(uint16_t mode)
{
    if (S_ISREG(mode))
        return 'f';
    else if (S_ISDIR(mode))
        return 'd';
    else if (S_ISCHR(mode))
        return 'c';
    else if (S_ISBLK(mode))
        return 'b';
    else if (S_ISFIFO(mode))
        return 'p';
    else if (S_ISLNK(mode))
        return 'l';
    else if (S_ISSOCK(mode))
        return 's';
    else
        return 'U';
}

static int
depth_from_path(const char *path)
{
    int count = 0;

    if (path[0] == '/' && path[1] == 0)
        return 0;

    for (int i = 0; path[i]; i++)
        if (path[i] == '/')
            count++;

    return count;
}

static void
symbolic_permission(char *symbolic_mode, mode_t mode)
{
    symbolic_mode[0] = type2char(mode);
    if (symbolic_mode[0] == 'f')
        symbolic_mode[0] = '-';

    static_assert(ARRAY_SIZE(MODE_BITS) == ARRAY_SIZE(SPECIAL_BITS), "");
    for (size_t i = 0; i < ARRAY_SIZE(MODE_BITS); i++) {
        const char *mapping =
            mode & SPECIAL_BITS[i] ? mode & MODE_BITS[i] ? "..s..s..t"
                                                         : "..S..S..T"
                                   : mode & MODE_BITS[i] ? "rwxrwxrwx"
                                                         : "---------";
        symbolic_mode[i + 1] = mapping[i];
    }

    symbolic_mode[10] = 0;
}

static const char *
remove_start_point(const char *path, const char *backend)
{
    size_t branch_len;
    char *branch;

    if (*path == '/' && *(path + 1) == 0)
        return "";

    // Get the branch point which is after the #
    branch = strchr(backend, '#');

    // If there is no branch, return the path without the '/'
    if (branch == NULL)
        return &path[1];

    // There is a branch for the find
    branch++;
    branch_len = strlen(branch);

    /* If the path after the branch is empty, it corresponds to the branch point
     * so we return an empty string.
     */
    if (path[branch_len + 1] == 0)
        return "";

    // Otherwise return the path after the branch and the '/'
    return &path[branch_len + 2];
}

int
fsentry_print_directive(char *output, int max_length,
                        const struct rbh_fsentry *fsentry,
                        const char *directive,
                        const char *backend)
{
    char symbolic_mode[11];
    int chars_written;
    const char *name;
    char *path;

    assert(directive != NULL);
    assert(*directive != '\0');

    /* For now, consider the directive to be a single character */
    switch (*directive) {
    case 'a':
        return snprintf(output, max_length, "%s",
                        time_from_timestamp(&fsentry->statx->stx_atime.tv_sec)
                        );
    case 'A':
        return snprintf(output, max_length, "%lu",
                        fsentry->statx->stx_atime.tv_sec);
    case 'b':
        return snprintf(output, max_length, "%lu", fsentry->statx->stx_blocks);
    case 'c':
        return snprintf(output, max_length, "%s",
                        time_from_timestamp(&fsentry->statx->stx_ctime.tv_sec)
                        );
    case 'd':
        return snprintf(output, max_length, "%d",
                        depth_from_path(fsentry_path(fsentry)));
    case 'D':
        return snprintf(output, max_length, "%lu",
                        makedev(fsentry->statx->stx_dev_major,
                                fsentry->statx->stx_dev_minor));
    case 'f':
        return snprintf(output, max_length, "%s", fsentry->name);
    case 'g':
        name = get_group_name(fsentry->statx->stx_gid);
        if (name)
            return snprintf(output, max_length, "%s", name);

        __attribute__((fallthrough));
    case 'G':
        return snprintf(output, max_length, "%u", fsentry->statx->stx_gid);
    case 'h':
        path = strdup(fsentry_path(fsentry));
        chars_written = snprintf(output, max_length, "%s", dirname(path));
        free(path);
        return chars_written;
    case 'H':
        return snprintf(output, max_length, "%s", backend);
    case 'i':
        return snprintf(output, max_length, "%lu", fsentry->statx->stx_ino);
    case 'l':
        if (!S_ISLNK(fsentry->statx->stx_mode))
            return 0;

        return snprintf(output, max_length, "%s", fsentry->symlink);
    case 'm':
        return snprintf(output, max_length, "%o",
                        fsentry->statx->stx_mode & 0777);
    case 'M':
        symbolic_permission(symbolic_mode, fsentry->statx->stx_mode);
        return snprintf(output, max_length, "%s", symbolic_mode);
    case 'n':
        return snprintf(output, max_length, "%d", fsentry->statx->stx_nlink);
    case 'p':
        return snprintf(output, max_length, "%s", fsentry_path(fsentry));
    case 'P':
        return snprintf(output, max_length, "%s",
                        remove_start_point(fsentry_path(fsentry), backend));
    case 's':
        return snprintf(output, max_length, "%lu", fsentry->statx->stx_size);
    case 't':
        return snprintf(output, max_length, "%s",
                        time_from_timestamp(&fsentry->statx->stx_mtime.tv_sec)
                        );
    case 'T':
        return snprintf(output, max_length, "%lu",
                        fsentry->statx->stx_mtime.tv_sec);
    case 'u':
        name = get_user_name(fsentry->statx->stx_uid);
        if (name)
            return snprintf(output, max_length, "%s", name);

        __attribute__((fallthrough));
    case 'U':
        return snprintf(output, max_length, "%u", fsentry->statx->stx_uid);
    case 'y':
        return snprintf(output, max_length, "%c",
                        type2char(fsentry->statx->stx_mode));
    case '%':
        return snprintf(output, max_length, "%%");
    default:
        error(EXIT_FAILURE, ENOTSUP, "format directive '%c' not supported",
              *directive);
    }

    __builtin_unreachable();
}

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
fsentry_printf_format(FILE *file, const struct rbh_fsentry *fsentry,
                      const char *format_string, const char *backend,
                      int (*print_directive)(char *, int,
                                             const struct rbh_fsentry *,
                                             const char *, const char *))
{
    size_t length = strlen(format_string);
    int max_length = MAX_OUTPUT_SIZE;
    char output[MAX_OUTPUT_SIZE];
    size_t output_length = 0;

    for (size_t i = 0; i < length; i++) {
        int tmp_length;

        switch (format_string[i]) {
        case '%':
            tmp_length = print_directive(&output[output_length], max_length,
                                         fsentry, format_string + i + 1,
                                         backend);
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
