/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
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

#define _debug(file, line, func, fmt, ...)          \
    ({                         \
      int save_errno = errno; \
      fprintf(stderr, "%s:%d:%s: ", (file), (line), (func)); \
      fprintf(stderr, fmt __VA_OPT__(,) __VA_ARGS__); \
      fprintf(stderr, "\n");   \
      errno = save_errno; \
    })

#define debug(fmt, ...) _debug(__FILE__, __LINE__, __func__, fmt, __VA_ARGS__)
#define entry() debug("entry")

/**
 * Create a backend from a URI string
 *
 * @param uri        the URI (a string) to use
 * @param read_only  whether we intend to open the backend in read only or
 *                   read/write mode
 *
 * @return           a pointer to a newly allocated backend on success,
 *                   exits on error.
 */
struct rbh_backend *
rbh_backend_from_uri(const char *uri, bool read_only);

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

/**
 * str2int64_t - convert a string into a int64_t
 *
 * @param input     a string representing a long
 * @param result    a int64_t represented by \p string
 *
 * @return          0 on success, -1 on error
 */
int
str2int64_t(const char *input, int64_t *result);

/**
 * str2uint64_t - convert a string into a uint64_t
 *
 * @param input     a string representing an unsigned long
 * @param result    a uint64_t represented by \p string
 *
 * @return          0 on success, -1 on error
 */
int
str2uint64_t(const char *input, uint64_t *result);

enum time_unit {
    TU_SECOND,
    TU_MINUTE,
    TU_HOUR,
    TU_DAY,
};

extern const unsigned long TIME_UNIT2SECONDS[];

/**
 * str2seconds - convert a string into a number of seconds
 *
 * @param unit      a time unit in which to interpret \p string
 * @param string    a string representing an unsigned long
 *
 * @return          an unsigned long representing \p string converted into
 *                  seconds
 */
unsigned long
str2seconds(enum time_unit unit, const char *string);

/**
 * count_char_separated_values - count the number of values separated by a
 * specific character
 *
 * @param str        a string
 * @param character  a character that separates the values
 *
 * @return           -1 if \p str is empty or there is an empty value,
 *                   number of occurences of \p character + 1 otherwise
 */
int
count_char_separated_values(const char *str, char character);

/**
 * Return the given timestamp as a string
 *
 * @param time       the timestamp to convert
 *
 * @return           \p time as a NULL-terminated string
 */
const char *
time_from_timestamp(const time_t *time);

/**
 * Formats a given size into a human-readable string
 *
 * @param buffer      buffer filled with the human-readable string
 * @param buffer_size size of the buffer
 * @param size        the size to be formatted by the function
 *
 * @return            number of characters written
 */
int
size_printer(char *buffer, size_t buffer_size, size_t size);

#endif
