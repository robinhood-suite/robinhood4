/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>

#include <robinhood/config.h>

#include "rbh-find/actions.h"
#include "rbh-find/utils.h"

#include "actions.h"

static int
write_expiration_date_from_entry(const struct rbh_fsentry *fsentry,
                                 char *output, int max_length)
{
    const struct rbh_value *value =
        rbh_fsentry_find_inode_xattr(fsentry, "trusted.expiration_date");

    if (value == NULL || value->type != RBH_VT_INT64)
        return snprintf(output, max_length, "None");
    else if (value->int64 == INT64_MAX)
        return snprintf(output, max_length, "Inf");
    else
        return snprintf(output, max_length, "%s",
                        time_from_timestamp(&value->int64));
}

static int
write_expires_from_entry(const struct rbh_fsentry *fsentry,
                         char *output, int max_length)
{
    static const char *retention_attribute;
    const struct rbh_value *value;

    if (retention_attribute == NULL)
        retention_attribute = rbh_config_get_string(XATTR_EXPIRES_KEY,
                                                    "user.expires");
    if (retention_attribute == NULL)
        return snprintf(output, max_length, "None");

    value = rbh_fsentry_find_inode_xattr(fsentry, retention_attribute);

    if (value == NULL || value->type != RBH_VT_STRING)
        return snprintf(output, max_length, "None");
    else
        return snprintf(output, max_length, "%s", value->string);
}

static const char table[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/',
};

size_t
base64_encode(char *dest, const char *src, size_t n)
{
    const uint8_t *data = (const unsigned char *)src;
    size_t i, j = 0;

    for (i = 0; i + 2 < n; i += 3) {
        dest[j++] = table[data[i] >> 2];
        dest[j++] = table[(data[i] & 0x3) << 4 | data[i + 1] >> 4];
        dest[j++] = table[(data[i + 1] & 0xf) << 2 | data[i + 2] >> 6];
        dest[j++] = table[data[i + 2] & 0x3f];
    }

    switch (n % 3) {
    case 0:
        break;
    case 1:
        dest[j++] = table[data[i] >> 2];
        dest[j++] = table[(data[i] & 0x3) << 4];
        dest[j++] = '=';
        dest[j++] = '=';
        break;
    case 2:
        dest[j++] = table[data[i] >> 2];
        dest[j++] = table[(data[i] & 0x3) << 4 | data[i + 1] >> 4];
        dest[j++] = table[(data[i + 1] & 0xf) << 2];
        dest[j++] = '=';
        break;
    }

    dest[j] = '\0';
    return j;
}

static int
write_base64_ID(const struct rbh_fsentry *fsentry, char *output, int max_length)
{
    char buffer[1024]; // More than enough to hold the converted ID

    if (base64_encode(buffer, fsentry->id.data, fsentry->id.size) == 0)
        return -1;

    return snprintf(output, max_length, "%s", buffer);
}

int
fsentry_print_lustre_directive(char *output, int max_length,
                               const struct rbh_fsentry *fsentry,
                               const char *directive,
                               const char *backend)
{
    assert(directive != NULL);
    assert(*directive != '\0');

    switch (*directive) {
    case 'e':
        return write_expires_from_entry(fsentry, output, max_length);
    case 'E':
        return write_expiration_date_from_entry(fsentry, output, max_length);
    case 'I':
        return write_base64_ID(fsentry, output, max_length);
    default:
        return fsentry_print_directive(output, max_length, fsentry, directive,
                                       backend);
    }

    __builtin_unreachable();
}
