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
#include <limits.h>

#include "robinhood/uri.h"
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

const unsigned long TIME_UNIT2SECONDS[] = {
    [TU_SECOND] = 1,
    [TU_MINUTE] = 60,
    [TU_HOUR] = 3600,
    [TU_DAY] = 86400,
};

unsigned long
str2seconds(enum time_unit unit, const char *string)
{
    unsigned long delta;
    char *endptr;

    delta = strtoul(string, &endptr, 10);
    if ((errno == ERANGE && delta == ULONG_MAX) || (errno != 0 && delta == 0))
        return delta;
    if (*endptr != '\0') {
        errno = EINVAL;
        return 0;
    }

    if (ULONG_MAX / TIME_UNIT2SECONDS[unit] < delta) {
        errno = ERANGE;
        return ULONG_MAX;
    }

    return delta * TIME_UNIT2SECONDS[unit];
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

const char *
time_from_timestamp(const time_t *time)
{
    char *res = ctime(time);
    size_t len;

    if (!res)
        /* ctime can return NULL if time is an invalid time (e.g. time < 0) */
        return "???";

    len = strlen(res);

    /* ctime adds an extra \n at then end of the buffer, remove it */
    res[len - 1] = '\0';

    return res;
}

int
size_printer(char *buffer, size_t buffer_size, size_t size)
{
    size_t sfx_value = (size_t)1 << 60;
    const char *sfx = "EPTGMK";
    int sfx_idx = 0;

    while (sfx_value > 1 && size < sfx_value) {
        sfx_value >>= 10;
        sfx_idx++;
    }

    if (sfx_value > 1)
        return snprintf(buffer, buffer_size, "%.2f %cB",
                        (double)size / sfx_value, sfx[sfx_idx]);
    else
        return snprintf(buffer, buffer_size, "%zu Bytes", size);
}

int
rbh_count_args_before_uri(int argc, char **argv)
{
    size_t count = 0;

    while (count < argc && !rbh_is_uri(argv[count]))
        count++;

    return count;
}
