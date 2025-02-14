/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_OPEN_H
#define ROBINHOOD_OPEN_H

#include "robinhood/id.h"

/**
 * Retrieve mount point from root's path
 *
 * @param root         root's path
 *
 * @return             positive int on success (file descriptor), -1 on error
 */
int
mount_fd_by_root(const char *root);

/**
 * Handles open_by_id with generic flags (O_RDONLY, O_CLOEXEC,
 * O_NOFOLLOW, O_NONBLOCK).
 *
 * @param mount_fd    mount point used to access the desired file
 * @param id          id used to retrieve handle (rbh_file_handle_from_id)
 *
 * @return            non negative int on success (fd), -1 on error
 */
int
open_by_id_generic(int mount_fd, const struct rbh_id *id);

/**
 * Handles open_by_id when there is no need to fully open the file
 *
 * @param mount_fd    mount point used to access the desired file
 * @param id          id used to retrieve handle (rbh_file_handle_from_id)
 *
 * @return            non negative int on success (fd), -1 on error
 */
int
open_by_id_opath(int mount_fd, const struct rbh_id *id);

#endif
