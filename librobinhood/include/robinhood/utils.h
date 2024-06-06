/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

/**
 * A collection of opinionated utility functions
 *
 * These functions provide reference implementations for basic use cases of
 * the RobinHood library. They may/should be used by third-party applications.
 *
 * Note that unlike their lower-level counterparts, the function provided here
 * make a number of design choices such as exiting on error and what error
 * messages to use.
 */

#ifndef ROBINHOOD_UTILS_H
#define ROBINHOOD_UTILS_H

#include "robinhood/backend.h"
#include "robinhood/config.h"

/**
 * Create a backend from a URI string
 *
 * @param uri   the URI (a string) to use
 *
 * @return      a pointer to a newly allocated backend on success,
 *              exits on error.
 */
struct rbh_backend *
rbh_backend_from_uri(const char *uri);

/**
 * Retrieves the mount path of a filesystem, given an entry under this mount
 * path.
 * If multiple mount points match, returns the most recent one.
 *
 * @param[in]  path      Path of an entry in a filesystem (must be absolute
 *                       and canonical for the result of this function to be
 *                       accurate)
 * @param[out] mnt_path  Pointer to the path where the filesystem is mounted
 *                       (must be freed by the caller).
 *
 * @return 0 on success, another value error and errno is set.
 */
int
get_mount_path(const char *path, char **mount_path);

/**
 * shell2pcre - translate a shell pattern into a Perl Compatible regex
 *
 * @param shell the shell pattern to convert
 *
 * @return      a pointer to the newly allocated perl compatible regex
 *
 * The conversion prefixes any '*' and '?' with a dot unless it is escaped.
 * A leading '^' and a trailing '(?!\n)$' are added to the pattern.
 * The '(?!\n)' part is meant as a workaround to PCRE's specification that
 * allows '$' to match either before or after the last trailing newline.
 *
 * The returned pointer must be freed by the caller.
 */
char *
shell2pcre(const char *shell);

#endif
