/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <error.h>
#include <grp.h>
#include <inttypes.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#include <robinhood/backend.h>
#include <robinhood/fsentry.h>
#include <robinhood/statx.h>
#include <robinhood/utils.h>

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

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

int
rbh_posix_delete_entry(struct rbh_fsentry *fsentry)
{
    return unlink(fsentry_relative_path(fsentry));
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
rbh_posix_fill_entry_info(char *output, int max_length,
                          const struct rbh_fsentry *fsentry,
                          const char *directive, const char *backend)
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
    }

    return 0;
}
