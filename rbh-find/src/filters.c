/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <robinhood/statx.h>
#ifndef HAVE_STATX
# include <robinhood/statx.h>
#endif

#include <robinhood/backend.h>
#include <robinhood/sstack.h>
#include <robinhood/utils.h>

#include "filters.h"

struct rbh_filter_field
str2field(const char *attribute)
{
    struct rbh_filter_field field;

    switch(attribute[0]) {
    case 'a':
        if (strcmp(&attribute[1], "time") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_ATIME_SEC;
            return field;
        }
        break;
    case 'b':
        switch (attribute[1]) {
        case 'l':
            if (strcmp(&attribute[2], "ocks"))
                break;
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_BLOCKS;
            return field;
        case 't':
            if (strcmp(&attribute[2], "ime"))
                break;
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_BTIME_SEC;
            return field;
        }
        break;
    case 'c':
        if (strcmp(&attribute[1], "time") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_CTIME_SEC;
            return field;
        }
        break;
    case 'g':
        if (strcmp(&attribute[1], "id") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_GID;
            return field;
        }
        break;
    case 'i':
        if (strcmp(&attribute[1], "no") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_INO;
            return field;
        }
        break;
    case 'm':
        switch (attribute[1]) {
        case 'o':
            if (strcmp(&attribute[2], "de"))
                break;
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_MODE;
            return field;
        case 't':
            if (strcmp(&attribute[2], "ime"))
                break;
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_MTIME_SEC;
            return field;
        }
        break;
    case 'n':
        switch (attribute[1]) {
        case 'a':
            if (strcmp(&attribute[2], "me"))
                break;
            field.fsentry = RBH_FP_NAME;
            return field;
        case 'l':
            if (strcmp(&attribute[2], "ink"))
                break;
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_NLINK;
            return field;
        }
        break;
    case 's':
        if (strcmp(&attribute[1], "ize") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_SIZE;
            return field;
        }
        break;
    case 't':
        if (strcmp(&attribute[1], "ype") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_TYPE;
            return field;
        }
        break;
    case 'u':
        if (strcmp(&attribute[1], "id") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = RBH_STATX_UID;
            return field;
        }
        break;
    }
    error(EX_USAGE, 0, "invalid field for sort: %s", attribute);
    __builtin_unreachable();
}

void
sort_options_append(struct rbh_filter_options *options,
                    struct rbh_filter_field field, bool ascending)
{
    struct rbh_filter_sort new_sort = {
        .field = field,
        .ascending = ascending
    };

    options->sort.items = reallocarray(options->sort.items,
                                       options->sort.count + 1,
                                       sizeof(*options->sort.items));
    if (options->sort.items == NULL)
        error(EXIT_FAILURE, errno, "reallocarray on sort array failed");

    options->sort.items[options->sort.count++] = new_sort;
}
