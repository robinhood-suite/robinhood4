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

void
size_printer(int64_t size)
{
    const int64_t EB = 1LL << 60;
    const int64_t PB = 1LL << 50;
    const int64_t TB = 1LL << 40;
    const int64_t GB = 1LL << 30;
    const int64_t MB = 1LL << 20;
    const int64_t KB = 1LL << 10;

    if (size >= EB)
        printf("Size: %.2f EB\n", (double)size / EB);
    else if (size >= PB)
        printf("Size: %.2f PB\n", (double)size / PB);
    else if (size >= TB)
        printf("Size: %.2f TB\n", (double)size / TB);
    else if (size >= GB)
        printf("Size: %.2f GB\n", (double)size / GB);
    else if (size >= MB)
        printf("Size: %.2f MB\n", (double)size / MB);
    else if (size >= KB)
        printf("Size: %.2f KB\n", (double)size / KB);
    else
        printf("Size: %ld bytes\n", size);
}
