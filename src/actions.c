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
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/param.h>
#include <sys/stat.h>

#include <robinhood/statx.h>

#include "rbh-find/actions.h"

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

const char *
fsentry_path(const struct rbh_fsentry *fsentry)
{
    if (!(fsentry->mask & RBH_FP_NAMESPACE_XATTRS))
        return NULL;

    for (size_t i = 0; i < fsentry->xattrs.ns.count; i++) {
        const struct rbh_value_pair *pair = &fsentry->xattrs.ns.pairs[i];

        if (strcmp(pair->key, "path"))
            continue;

        if (pair->value->type != RBH_VT_STRING)
            /* XXX: should probably say something... */
            continue;

        return pair->value->string;
    }

    return NULL;
}

#define MAX_OUTPUT_SIZE (PATH_MAX + 256)

static int
fsentry_print_directive(char *output, int max_length,
                        const struct rbh_fsentry *fsentry,
                        const char *directive)
{
    assert(directive != NULL);
    assert(*directive != '\0');

    /* For now, consider the directive to be a single character */
    switch (*directive) {
    case 'p':
        return snprintf(output, max_length, "%s", fsentry_path(fsentry));
    default:
        error(EXIT_FAILURE, ENOTSUP, "format directive not supported");
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

void
fsentry_printf_format(FILE *file, const struct rbh_fsentry *fsentry,
                      const char *format_string)
{
    size_t length = strlen(format_string);
    int max_length = MAX_OUTPUT_SIZE;
    char output[MAX_OUTPUT_SIZE];
    size_t output_length = 0;

    for (size_t i = 0; i < length; i++) {
        int tmp_length;

        switch (format_string[i]) {
        case '%':
            tmp_length = fsentry_print_directive(&output[output_length],
                                                 max_length, fsentry,
                                                 format_string + i + 1);
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
            error(EXIT_FAILURE, ENOTSUP, "char in format string not supported");
        }

        max_length -= tmp_length;

        if (tmp_length < 0) {
            error(EXIT_FAILURE, EINVAL, "failed to write to buffer for printf");
        } else if (max_length <= 0) {
            /* No more data can fit in the output array */
            output_length = MAX_OUTPUT_SIZE - 1;
            break;
        }

        output_length += tmp_length;
    }

    output[output_length] = 0;
    fprintf(file, "%s", output);
}
