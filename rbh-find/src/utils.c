/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
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

const char *
time_from_timestamp(const time_t *time)
{
    char *res = ctime(time);
    size_t len = strlen(res);

    /* ctime adds an extra \n at then end of the buffer, remove it */
    res[len - 1] = '\0';

    return res;
}
