/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "rbh-find/utils.h"

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

const char *
time_from_timestamp(const time_t *time)
{
    char *res = ctime(time);
    size_t len = strlen(res);

    /* ctime adds an extra \n at then end of the buffer, remove it */
    res[len - 1] = '\0';

    return res;
}
