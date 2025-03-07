/* This file is part of the RobinHood Library
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>

#include "robinhood/utils.h"

int
str2int64_t(const char *input, int64_t *result)
{
    char *end;

    errno = 0;
    *result = strtol(input, &end, 0);
    if (errno) {
        return -1;
    } else if ((!*result && input == end) || *end != '\0') {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

int
str2uint64_t(const char *input, uint64_t *result)
{
    char *end;

    errno = 0;
    *result = strtoull(input, &end, 0);
    if (errno) {
        return -1;
    } else if ((!*result && input == end) || *end != '\0') {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

int
count_char_separated_values(const char *str, char character)
{
    int count;

    if (str == NULL || *str == '\0' || *str == character)
        return -1;

    /* Even if the string doesn't have any of the requested character, it still
     * has at least one value.
     */
    count = 1;

    str++;
    while (*str) {
        if (*str == character) {
            if (*(str + 1) == character || *(str + 1) == '\0')
                return -1;

            count++;
        }

        str++;
    }

    return count;
}

char *
size_printer(char *buffer, size_t size)
{
    size_t buffer_size = sizeof(buffer);
    const int64_t EB = (size_t)1 << 60;
    const int64_t PB = (size_t)1 << 50;
    const int64_t TB = (size_t)1 << 40;
    const int64_t GB = (size_t)1 << 30;
    const int64_t MB = (size_t)1 << 20;
    const int64_t KB = (size_t)1 << 10;

    if (size >= EB)
        snprintf(buffer, buffer_size, "%.2ld EB", size / EB);
    else if (size >= PB)
        snprintf(buffer, buffer_size, "%.2ld PB", size / PB);
    else if (size >= TB)
        snprintf(buffer, buffer_size, "%.2ld TB", size / TB);
    else if (size >= GB)
        snprintf(buffer, buffer_size, "%.2ld GB", size / GB);
    else if (size >= MB)
        snprintf(buffer, buffer_size, "%.2ld MB", size / MB);
    else if (size >= KB)
        snprintf(buffer, buffer_size, "%.2ld KB", size / KB);
    else
        snprintf(buffer, buffer_size, "%.2ld Bytes", size);

    return buffer;
}
