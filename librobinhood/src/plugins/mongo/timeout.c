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
#define MONGODB_CURSOR_TIMEOUT       "cursor_timeout"

#define READ_SOCKET_TIMEOUT_DEFAULT  0
#define WRITE_SOCKET_TIMEOUT_DEFAULT 600000

/* Search 'key' in the configuration and store the corresponding value in
 * 'value_out' (only when found).
 *
 * The parse result and the value travel on separate channels on purpose: a
 * configured timeout of 1 ms is a legal value, not a KPR_NOT_FOUND sentinel.
 */
static enum key_parse_result
find_timeout_key(const char *key, int32_t *value_out)
{
    struct rbh_value value = { 0 };
    enum key_parse_result rc;

    rc = rbh_config_find(key, &value, RBH_VT_INT32);
    if (rc == KPR_ERROR)
        return KPR_ERROR;

    if (rc == KPR_FOUND) {
        if (value.int32 < 0) {
            fprintf(stderr,
                    "mongo: '%s' must be a non-negative number of milliseconds\n",
                    key);
            errno = EINVAL;
            return KPR_ERROR;
        }
        *value_out = value.int32;
    }

    return rc;
}

int32_t
mongo_socket_timeout_from_config(bool read_only)
{
    enum key_parse_result rc;
    int32_t timeout = 0;

    rc = find_timeout_key(read_only
                          ? "mongo/"MONGODB_READ_SOCKET_TIMEOUT
                          : "mongo/"MONGODB_WRITE_SOCKET_TIMEOUT,
                          &timeout);
    if (rc == KPR_ERROR)
        return -1;

    if (rc == KPR_NOT_FOUND) {
        /* Fall back on the deprecated 'cursor_timeout' option, which applied
         * to every backend opening mode.
         */
        rc = find_timeout_key("mongo/"MONGODB_CURSOR_TIMEOUT, &timeout);
        if (rc == KPR_ERROR)
            return -1;

        if (rc == KPR_FOUND)
            fprintf(stderr,
                    "mongo: '%s' is deprecated, use '%s' instead\n",
                    "mongo/"MONGODB_CURSOR_TIMEOUT,
                    read_only ? "mongo/"MONGODB_READ_SOCKET_TIMEOUT
                              : "mongo/"MONGODB_WRITE_SOCKET_TIMEOUT);
        else
            timeout = read_only ? READ_SOCKET_TIMEOUT_DEFAULT
                                : WRITE_SOCKET_TIMEOUT_DEFAULT;
    }

    /* libmongoc 1.24 replaces a socketTimeoutMS of 0 with its own default
     * (5 minutes) instead of disabling the timeout, so a configured value of
     * 0 is converted to INT32_MAX.
     */
    if (timeout == 0)
        timeout = INT32_MAX;

    return timeout;
}
