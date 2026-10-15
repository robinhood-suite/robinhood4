/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <robinhood/config.h>

#include "timeout.h"

#define MONGODB_READ_SOCKET_TIMEOUT  "read_socket_timeout"
#define MONGODB_WRITE_SOCKET_TIMEOUT "write_socket_timeout"

#define READ_SOCKET_TIMEOUT_DEFAULT  0
#define WRITE_SOCKET_TIMEOUT_DEFAULT 600000

static int32_t
find_timeout_key(const char *key)
{
    struct rbh_value value = { 0 };
    enum key_parse_result rc;

    rc = rbh_config_find(key, &value, RBH_VT_INT32);
    if (rc == KPR_ERROR)
        return -1;

    if (rc == KPR_FOUND) {
        if (value.int32 < 0) {
            fprintf(stderr,
                    "mongo: '%s' must be a positive number of milliseconds\n",
                    key);
            errno = EINVAL;
            return -1;
        }
        return value.int32;
    }

    return KPR_NOT_FOUND;
}

int32_t
mongo_socket_timeout_from_config(bool read_only)
{
    int32_t timeout;

    timeout = find_timeout_key(read_only
                               ? "mongo/"MONGODB_READ_SOCKET_TIMEOUT
                               : "mongo/"MONGODB_WRITE_SOCKET_TIMEOUT);
    if (timeout == -1)
        return -1;

    if (timeout == KPR_NOT_FOUND)
        timeout = read_only ? READ_SOCKET_TIMEOUT_DEFAULT
                            : WRITE_SOCKET_TIMEOUT_DEFAULT;

    /* libmongoc 1.24 replaces a socketTimeoutMS of 0 with its own default
     * (5 minutes) instead of disabling the timeout, so a configured value of
     * 0 is converted to INT32_MAX.
     */
    if (timeout == 0)
        timeout = INT32_MAX;

    return timeout;
}
