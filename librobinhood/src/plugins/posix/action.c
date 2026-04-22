/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <assert.h>
#include <errno.h>
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

#include <robinhood/action.h>
#include <robinhood/backend.h>
#include <robinhood/fsentry.h>
#include <robinhood/projection.h>
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

/* Build the absolute floor path:
 * - If parents_below is given, use it as the floor relative to the mountpoint.
 * - Otherwise, set the floor to the grandparent of the deleted entry so that
 *   only the direct parent can be removed. */
static char *
build_abs_floor(const char *path, size_t mountpoint_len,
                const char *parents_below)
{
    char *abs_floor;

    if (parents_below) {
        size_t floor_len = mountpoint_len + 1 + strlen(parents_below);
        abs_floor = xmalloc(floor_len + 1);
        memcpy(abs_floor, path, mountpoint_len);
        abs_floor[mountpoint_len] = '/';
        strcpy(abs_floor + mountpoint_len + 1, parents_below);
    } else {
        char *tmp1 = xstrdup(path);
        char *tmp2 = xstrdup(dirname(tmp1));
        abs_floor = xstrdup(dirname(tmp2));
        free(tmp1);
        free(tmp2);
    }

    return abs_floor;
}

static enum rbh_delete_action_return
remove_empty_parents(const char *path, size_t mountpoint_len,
                     const char *abs_floor)
{
    char *current = xstrdup(path);
    bool parent_removed = false;

    while (true) {
        char *tmp = xstrdup(current);
        char *parent = dirname(tmp);

        /* Stop at or above the mountpoint */
        if (strlen(parent) <= mountpoint_len) {
            free(tmp);
            break;
        }

        /* Stop at the floor path (do not remove it) */
        if (abs_floor && strcmp(parent, abs_floor) == 0) {
            free(tmp);
            break;
        }

        if (rmdir(parent) == 0) {
            parent_removed = true;
            free(current);
            current = tmp;
        } else {
            /* ENOTEMPTY means the directory is not empty; this is not really
             * an error, just a stopping condition. Other errors should be
             * signified to the user. */
            if (errno != ENOTEMPTY) {
                free(tmp);
                free(current);

                return RBH_DELETE_ERROR;
            }

            free(tmp);
            break;
        }
    }

    free(current);
    return parent_removed ? RBH_DELETE_OK_WITH_PARENTS : RBH_DELETE_OK;
}

int
rbh_posix_delete_entry(struct rbh_backend *backend,
                       struct rbh_fsentry *fsentry,
                       const struct rbh_delete_params *params)
{
    bool remove_empty_parent = false;
    const char *parents_below = NULL;
    char *path;
    int rc;

    if (params) {
        remove_empty_parent = params->remove_empty_parent;
        parents_below = params->remove_parents_below;
    }
    path = fsentry_absolute_path(backend, fsentry);

    if (S_ISDIR(fsentry->statx->stx_mode))
        rc = rmdir(path);
    else
        rc = unlink(path);

    if (rc == 0 && remove_empty_parent) {
        const char *rel_path = fsentry_relative_path(fsentry);
        bool parent_is_mountpoint = false;

        /* Do not attempt to remove the parent if it is the backend mount
         * point. Since rel_path is relative to the mountpoint, checking if
         * its dirname equals "." means we're attempting to delete from the
         * mountpoint itself. */
        if (rel_path != NULL) {
            char *rel_path_copy = xstrdup(rel_path);
            char *parent = dirname(rel_path_copy);
            /* If dirname is ".", the entry is at the mountpoint root */
            parent_is_mountpoint = (strcmp(parent, ".") == 0);
            free(rel_path_copy);
        } else {
            parent_is_mountpoint = true;
        }

        if (!parent_is_mountpoint) {
            /* mountpoint_len is the length of the absolute mountpoint path,
             * e.g. len("/mnt/fs") when abs_path is "/mnt/fs/a/b/c" and
             * rel_path is "a/b/c". */
            size_t mountpoint_len = strlen(path) - strlen(rel_path) - 1;
            enum rbh_delete_action_return result;
            char *abs_floor;

            abs_floor = build_abs_floor(path, mountpoint_len, parents_below);
            result = remove_empty_parents(path, mountpoint_len, abs_floor);

            rc = result;

            free(abs_floor);
        }
    }

    free(path);
    return rc;
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

enum known_directive
rbh_posix_fill_entry_info(const struct rbh_fsentry *fsentry,
                          const char *format_string, size_t *index,
                          char *output, size_t *output_length, int max_length,
                          const char *backend)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;
    char symbolic_mode[11];
    int tmp_length = 0;
    const char *name;

    switch (format_string[*index + 1]) {
    case 'a':
        tmp_length = snprintf(
            output, max_length,
            "%s", time_from_timestamp(&fsentry->statx->stx_atime.tv_sec)
        );
        break;
    case 'A':
        tmp_length = snprintf(output, max_length,
                              "%lu", fsentry->statx->stx_atime.tv_sec);
        break;
    case 'b':
        tmp_length = snprintf(output, max_length,
                              "%lu", fsentry->statx->stx_blocks);
        break;
    case 'c':
        tmp_length = snprintf(
            output, max_length,
            "%s", time_from_timestamp(&fsentry->statx->stx_ctime.tv_sec)
        );
        break;
    case 'D':
        tmp_length = snprintf(output, max_length,
                              "%lu", makedev(fsentry->statx->stx_dev_major,
                                             fsentry->statx->stx_dev_minor));
        break;
    case 'g':
        name = get_group_name(fsentry->statx->stx_gid);
        if (name) {
            tmp_length = snprintf(output, max_length, "%s", name);
            break;
        }

        __attribute__((fallthrough));
    case 'G':
        tmp_length = snprintf(output, max_length,
                              "%u", fsentry->statx->stx_gid);
        break;
    case 'i':
        tmp_length = snprintf(output, max_length,
                              "%lu", fsentry->statx->stx_ino);
        break;
    case 'l':
        if (!S_ISLNK(fsentry->statx->stx_mode))
            tmp_length = snprintf(output, max_length, "None");
        else
            tmp_length = snprintf(output, max_length, "%s", fsentry->symlink);
        break;
    case 'm':
        tmp_length = snprintf(output, max_length, "%o",
                              fsentry->statx->stx_mode & 0777);
        break;
    case 'M':
        symbolic_permission(symbolic_mode, fsentry->statx->stx_mode);
        tmp_length = snprintf(output, max_length, "%s", symbolic_mode);
        break;
    case 'n':
        tmp_length = snprintf(output, max_length,
                              "%d", fsentry->statx->stx_nlink);
        break;
    case 's':
        tmp_length = snprintf(output, max_length,
                              "%lu", fsentry->statx->stx_size);
        break;
    case 't':
        tmp_length = snprintf(
            output, max_length,
            "%s", time_from_timestamp(&fsentry->statx->stx_mtime.tv_sec)
        );
        break;
    case 'T':
        tmp_length = snprintf(output, max_length,
                              "%lu", fsentry->statx->stx_mtime.tv_sec);
        break;
    case 'u':
        name = get_user_name(fsentry->statx->stx_uid);
        if (name) {
            tmp_length = snprintf(output, max_length, "%s", name);
            break;
        }

        __attribute__((fallthrough));
    case 'U':
        tmp_length = snprintf(output, max_length,
                              "%u", fsentry->statx->stx_uid);
        break;
    case 'y':
        tmp_length = snprintf(output, max_length,
                              "%c", type2char(fsentry->statx->stx_mode));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (tmp_length < 0)
        return RBH_DIRECTIVE_ERROR;

    if (rc == RBH_DIRECTIVE_KNOWN) {
        *output_length += tmp_length;
        (*index)++;
    }

    return rc;
}

enum known_directive
rbh_posix_fill_projection(struct rbh_filter_projection *projection,
                          const char *format_string, size_t *index)
{
    enum known_directive rc = RBH_DIRECTIVE_KNOWN;

    switch (format_string[*index + 1]) {
    case 'a':
    case 'A':
        rbh_projection_add(projection, str2filter_field("statx.atime.sec"));
        break;
    case 'b':
        rbh_projection_add(projection, str2filter_field("statx.blocks"));
        break;
    case 'c':
        rbh_projection_add(projection, str2filter_field("statx.ctime.sec"));
        break;
    case 'D':
        rbh_projection_add(projection, str2filter_field("statx.dev.minor"));
        rbh_projection_add(projection, str2filter_field("statx.dev.major"));
        break;
    case 'g':
    case 'G':
        rbh_projection_add(projection, str2filter_field("statx.gid"));
        break;
    case 'i':
        rbh_projection_add(projection, str2filter_field("statx.ino"));
        break;
    case 'l':
        rbh_projection_add(projection, str2filter_field("statx.type"));
        rbh_projection_add(projection, str2filter_field("statx.mode"));
        rbh_projection_add(projection, str2filter_field("symlink"));
        break;
    case 'm':
    case 'M':
    case 'y':
        rbh_projection_add(projection, str2filter_field("statx.type"));
        rbh_projection_add(projection, str2filter_field("statx.mode"));
        break;
    case 'n':
        rbh_projection_add(projection, str2filter_field("statx.nlink"));
        break;
    case 's':
        rbh_projection_add(projection, str2filter_field("statx.size"));
        break;
    case 't':
    case 'T':
        rbh_projection_add(projection, str2filter_field("statx.mtime.sec"));
        break;
    case 'u':
    case 'U':
        rbh_projection_add(projection, str2filter_field("statx.uid"));
        break;
    default:
        rc = RBH_DIRECTIVE_UNKNOWN;
    }

    if (rc == RBH_DIRECTIVE_KNOWN)
        (*index)++;

    return rc;
}
