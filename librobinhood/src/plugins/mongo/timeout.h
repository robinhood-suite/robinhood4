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
 *  - read-only backends (rbh-find, rbh-report, ...) use
 *    `mongo/read_socket_timeout`
 *  - write backends (rbh-fsevents and its checkpoints, ...) use
 *    `mongo/write_socket_timeout`
 *
 * @param read_only   the opening mode of the backend being created
 *
 * @return            the value to set for MONGOC_URI_SOCKETTIMEOUTMS,
 *                    or negative value on error and errno set
 */
int32_t
mongo_socket_timeout_from_config(bool read_only);

#endif
