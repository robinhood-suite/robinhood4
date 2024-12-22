/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/xattr.h>

#include "retention_internals.h"
#include "robinhood/statx.h"
#include "robinhood/config.h"
#include "robinhood/backends/common.h"
#include "sstack.h"
#include "value.h"

/* Per entry list of xattrs. If set to NULL, the value of the retention will
 * be retrieved from the file's xattr.
 */
static __thread struct rbh_value_pair *_inode_xattrs;
static __thread ssize_t *_inode_xattrs_count;

/* stack to allocate memory for rbh_value_pairs */
static __thread struct rbh_sstack *_values;

/* Cached value of the configuration's retention attribute name */
static const char *_retention_attribute;

static int64_t
parse_user_expiration_date(const char *user_retention,
                          const struct rbh_statx *statx)
{
    int64_t expiration_date;
    char *end;

    switch (user_retention[0]) {
    case 'i':
        if (strcmp(user_retention + 1, "nf")) {
            fprintf(stderr,
                    "Invalid value for expiration attribute '%s', should be 'inf'\n",
                    user_retention);
            errno = EINVAL;
            return -1;
        }

        expiration_date = INT64_MAX;
        break;
    case '+':
        errno = 0;
        expiration_date = strtoul(user_retention + 1, &end, 10);
        if (errno) {
            fprintf(stderr,
                    "Invalid value for expiration attribute '%s', should be '+<integer>'\n",
                    user_retention);
            return -1;
        }

        if ((!expiration_date && user_retention == end) || *end != '\0') {
            fprintf(stderr,
                    "Invalid value for expiration attribute '%s', should be '+<integer>'\n",
                    user_retention);
            errno = EINVAL;
            return -1;
        }

        if (INT64_MAX - statx->stx_mtime.tv_sec < expiration_date)
            /* If the result overflows, set the expiration date to the max */
            expiration_date = INT64_MAX;
        else
            expiration_date += statx->stx_mtime.tv_sec;

        break;
    default:
        errno = 0;
        expiration_date = strtoul(user_retention, &end, 10);
        if (errno) {
            fprintf(stderr,
                    "Invalid value for expiration attribute '%s', should be an integer\n",
                    user_retention);
            return -1;
        }

        if ((!expiration_date && user_retention == end) || *end != '\0') {
            fprintf(stderr,
                    "Invalid value for expiration attribute '%s', should be an integer\n",
                    user_retention);
            errno = EINVAL;
            return -1;
        }
    }

    return expiration_date;
}

#define XATTR_EXPIRATION_DATE "trusted.expiration_date"

static int
create_expiration_date_value_pair(const char *attribute_value,
                                  const struct rbh_statx *statx,
                                  struct rbh_value_pair *expiration_pair)
{
    int64_t expiration_date = parse_user_expiration_date(attribute_value, statx);

    if (expiration_date < 0)
        return -1;

    fill_int64_pair(XATTR_EXPIRATION_DATE, expiration_date,
                    expiration_pair, _values);

    return 0;
}

static int
update_or_cast_expiration_date(struct rbh_value_pair *expiration_date,
                               char *user_retention,
                               const struct rbh_statx *statx)
{
    int64_t current_expiration_date;
    int64_t user_expiration_date;
    const char *expiration_value;
    char *end;

    user_expiration_date = parse_user_expiration_date(user_retention,
                                                      statx);
    if (user_expiration_date < 0)
        return -1;

    expiration_value = RBH_SSTACK_PUSH(_values,
                                       expiration_date->value->binary.data,
                                       expiration_date->value->binary.size);

    errno = 0;
    current_expiration_date = strtoul(expiration_value, &end, 10);
    if (errno ||
        (!current_expiration_date && expiration_value == end) ||
        (unsigned long) (end - expiration_value) !=
                         expiration_date->value->binary.size) {
        fprintf(stderr,
                "Invalid value for expiration date '%s', should be '<integer>'\n",
                expiration_value);
        errno = EINVAL;
        return -1;
    }

    if (user_expiration_date > current_expiration_date)
        fill_int64_pair(XATTR_EXPIRATION_DATE, user_expiration_date,
                        expiration_date, _values);
    else
        fill_int64_pair(XATTR_EXPIRATION_DATE, current_expiration_date,
                        expiration_date, _values);

    return 0;
}

#define INT64_MAX_STR_LEN 19

static int
copy_int64_str_from_binary(char *buffer, const struct rbh_value *value)
{
    assert(value->type == RBH_VT_BINARY);

    if (value->binary.size >= INT64_MAX_STR_LEN) {
        fprintf(stderr,
                "Invalid value for expiration attribute '%.*s', too long, max size is '%d'\n",
                (int) value->binary.size,
                value->binary.data,
                INT64_MAX_STR_LEN);
        errno = EINVAL;
        return -1;
    }

    memcpy(buffer, value->binary.data, value->binary.size);
    buffer[value->binary.size] = 0;

    return 0;
}

static int
enrich_from_file(int fd, const struct rbh_statx *statx,
                 struct rbh_value_pair *pairs)
{
    char user_retention[INT64_MAX_STR_LEN];
    ssize_t xattr_len;

    xattr_len = fgetxattr(fd, _retention_attribute, user_retention,
                          sizeof(user_retention));
    if (xattr_len == -1)
        return 0;

    user_retention[xattr_len] = '\0';

    if (create_expiration_date_value_pair(user_retention, statx, &pairs[0]))
        return 0;

    fill_string_pair(_retention_attribute, user_retention, &pairs[1], _values);

    return 2;
}

static struct rbh_value_pair *
find_xattr(const char *key, struct rbh_value_pair *pairs, ssize_t count)
{
    for (ssize_t i = 0; i < count; i++) {
        struct rbh_value_pair *xattr = &pairs[i];

        if (!strcmp(key, xattr->key))
            return xattr;
    }

    return NULL;
}

static int
enrich_from_xattrs(const struct rbh_statx *statx, struct rbh_value_pair *pairs)
{
    struct rbh_value_pair *expiration_date_xattr;
    struct rbh_value_pair *retention_xattr;
    char user_retention[INT64_MAX_STR_LEN];

    expiration_date_xattr = find_xattr(XATTR_EXPIRATION_DATE, _inode_xattrs,
                                       *_inode_xattrs_count);
    retention_xattr = find_xattr(_retention_attribute, _inode_xattrs,
                                 *_inode_xattrs_count);

    if (!retention_xattr)
        /* No retention set on the file, nothing to do */
        return 0;

    assert(retention_xattr->value->type == RBH_VT_BINARY);
    assert(!expiration_date_xattr ||
           expiration_date_xattr->value->type == RBH_VT_BINARY);

    copy_int64_str_from_binary(user_retention, retention_xattr->value);

    if (!expiration_date_xattr) {
        if (create_expiration_date_value_pair(user_retention, statx,
                                              &pairs[0]))
            return 0;
    } else {
        if (update_or_cast_expiration_date(expiration_date_xattr,
                                           user_retention, statx))
            return 0;
    }

    fill_string_pair(_retention_attribute, user_retention, retention_xattr,
                     _values);

    if (!expiration_date_xattr)
        return 1;

    return 0;
}

int
rbh_retention_enrich(struct entry_info *einfo, struct rbh_value_pair *pairs,
                     struct rbh_sstack *values)
{
    if (!_retention_attribute)
        _retention_attribute = rbh_config_get_string(XATTR_EXPIRES_KEY,
                                                     "user.expires");
    fprintf(stderr, "retention attr: %s\n", _retention_attribute);
    _inode_xattrs = einfo->inode_xattrs;
    _inode_xattrs_count = einfo->inode_xattrs_count;
    _values = values;

    if (!_inode_xattrs_count)
        return enrich_from_file(einfo->fd, einfo->statx, pairs);
    else
        return enrich_from_xattrs(einfo->statx, pairs);
}

int
rbh_retention_setup(void)
{
    // probably to init config
    return 0;
}
