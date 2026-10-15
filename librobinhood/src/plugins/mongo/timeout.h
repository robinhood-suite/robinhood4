/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_MONGO_TIMEOUT_H
#define RBH_MONGO_TIMEOUT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Resolve the socket timeout to configure on a mongo backend, in
 * milliseconds, from the configuration file.
 *
 * The backend opening mode selects the configuration key:
 *  - read-only backends (rbh-find, rbh-report, rbh-info, ...) use
 *    `mongo/read_socket_timeout`
 *  - write backends (rbh-sync, rbh-fsevents, rbh-gc, rbh-stats,
 *    rbh-undelete, rbh-update-path, ...) use `mongo/write_socket_timeout`.
 *    Note that tools like rbh-gc or rbh-stats are read-dominated but still
 *    open their backend in write mode, and therefore fall under
 *    `write_socket_timeout`.
 *
 * When the mode-specific key is not set, the deprecated `mongo/cursor_timeout`
 * option is used instead (with a warning), as it applied to every opening
 * mode.
 *
 * @param read_only   the opening mode of the backend being created
 *
 * @return            the value to set for MONGOC_URI_SOCKETTIMEOUTMS,
 *                    or negative value on error and errno set
 */
int32_t
mongo_socket_timeout_from_config(bool read_only);

#endif
