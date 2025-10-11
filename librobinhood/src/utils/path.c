/* This file is part of the RobinHood Library
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <mntent.h>
#include <stdlib.h>
#include <string.h>

#include "robinhood/utils.h"

int
get_mount_path(const char *path, char **mount_path)
{
    struct mntent mount_entry;
    char *mount_point = NULL;
    /* Buffer to store mount_entry. This size is consistent with the
     * non-rentrant version getmntent().
     */
    char mount_buffer[4096];
    struct mntent *p_mount;
    int save_errno;
    int pathlen;
    int rc = 0;
    FILE *fp;

    /* Check that path is absolute; note path could still not be canonical */
    if (path[0] != '/') {
        errno = EINVAL;
        return -1;
    }

    /* open mount tab to look for the given path */
    fp = setmntent(MOUNTED, "r");
    if (fp == NULL)
        return -1;

    while ((p_mount = getmntent_r(fp, &mount_entry, mount_buffer,
                                  sizeof(mount_buffer))) != NULL) {
        if (p_mount->mnt_dir == NULL)
            continue;

        /* Don't match root FS, we expect a mounted FS */
        if (!strcmp(p_mount->mnt_dir, "/"))
            continue;

        pathlen = strlen(p_mount->mnt_dir);

        /* Note that the right mount point is not necessarily the longest one,
         * but it is the last matching mnt ent, as the MTAB is ordered
         * chronologically.
         * For example, given the following mount sequence:
         *  Mount /var
         *  Mount /var/log
         *  Mount /var (again)
         *      The currently mounted filesystem is the last one (it hides the
         *      previous contents of /var).
         */
        /* The filesystem must be <mountpoint>/<smthg> or <mountpoint>\0 */
        if (!strncmp(path, p_mount->mnt_dir, pathlen) &&
            ((path[pathlen] == '/') || (path[pathlen] == '\0'))) {
            /* replace contents of mount_path */
            free(mount_point);
            mount_point = xstrdup(p_mount->mnt_dir);
        }
    }

    endmntent(fp);
    save_errno = errno;

    if (rc) {
        free(mount_point);
        *mount_path = NULL;
    } else if (mount_point == NULL) {
        rc = -1;
        errno = ENOENT;
    } else {
        *mount_path = mount_point;
    }

    errno = save_errno;

    return rc;
}
