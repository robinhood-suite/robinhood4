/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

#include "robinhood/open.h"

int
mount_fd_by_root(const char *root)
{
    int mount_fd;

    mount_fd = open(root, O_RDONLY | O_CLOEXEC);
    if (mount_fd < 0)
        return -1;

    return mount_fd;
}

int
open_by_id(int mount_fd, const struct rbh_id *id, int flags)
{
    struct file_handle *handle;
    int save_errno;
    int fd;

    handle = rbh_file_handle_from_id(id);
    if (handle == NULL)
        return -1;

    fd = open_by_handle_at(mount_fd, handle, flags);
    save_errno = errno;
    free(handle);
    errno = save_errno;
    return fd;
}

int
open_by_id_generic(int mount_fd, const struct rbh_id *id)
{
    return open_by_id(mount_fd, id,
                      O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
}

int
open_by_id_opath(int mount_fd, const struct rbh_id *id)
{
    return open_by_id(mount_fd, id,
                      O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_PATH);
}
