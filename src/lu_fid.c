/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

#include "lu_fid.h"

/*----------------------------------------------------------------------------*
 |                         lu_fid_init_from_string()                          |
 *----------------------------------------------------------------------------*/

static uint64_t
strtou64(const char *nptr, char **endptr, int base)
{
#if ULLONG_MAX == UINT64_MAX
    return strtoull(nptr, endptr, base);
#else /* ULLONG_MAX > UINT64_MAX */
    unsigned long long value;
    int save_errno = errno;

    errno = 0;
    value = strtoull(nptr, endptr, base);
    if (value == ULLONG_MAX && errno != 0)
        return UINT64_MAX;

    if (value > UINT64_MAX) {
        errno = ERANGE;
        return UINT64_MAX;
    }

    errno = save_errno;
    return value;
#endif
}

static uint32_t
strtou32(const char *nptr, char **endptr, int base)
{
#if ULONG_MAX == UINT32_MAX
    return strtoul(nptr, endptr, base);
#else /* ULONG_MAX > UINT32_MAX */
    unsigned long value;
    int save_errno = errno;

    errno = 0;
    value = strtoul(nptr, endptr, base);
    if (value == ULONG_MAX && errno != 0)
        return UINT32_MAX;

    if (value > UINT32_MAX) {
        errno = ERANGE;
        return UINT32_MAX;
    }

    errno = save_errno;
    return value;
#endif
}

int
lu_fid_init_from_string(const char *string, struct lu_fid *fid, char **endptr)
{
    int save_errno = errno;
    bool bracket = false;
    char *end;

    if (*string == '[') {
        bracket = true;
        string++;
    }

    errno = 0;
    fid->f_seq = strtou64(string, &end, 0);
    if (fid->f_seq == UINT64_MAX && errno != 0)
        return -1;
    if (*end != ':') {
        errno = EINVAL;
        return -1;
    }
    string = end + 1;

    errno = 0;
    fid->f_oid = strtou32(string, &end, 0);
    if (fid->f_oid == UINT32_MAX && errno != 0)
        return -1;
    if (*end != ':') {
        errno = EINVAL;
        return -1;
    }
    string = end + 1;

    errno = 0;
    fid->f_ver = strtou32(string, &end, 0);
    if (fid->f_ver == UINT32_MAX && errno != 0)
        return -1;

    if (bracket && *end++ != ']') {
        errno = EINVAL;
        return -1;
    }

    *endptr = end;
    errno = save_errno;
    return 0;
}
