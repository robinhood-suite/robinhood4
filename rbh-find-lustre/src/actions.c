/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <grp.h>
#include <inttypes.h>
#include <linux/limits.h>
#include <libgen.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <robinhood/statx.h>

#include "rbh-find/actions.h"

#include "actions.h"

static int
write_expiration_date_from_entry(const struct rbh_fsentry *fsentry,
                                 char *output, int max_length)
{
    const struct rbh_value *value =
        rbh_fsentry_find_inode_xattr(fsentry, "user.ccc_expiration_date");

    if (value != NULL)
        fprintf(stderr, "%s (%d): value type = '%d'\n", __FILE__, __LINE__,
                value->type);

    if (value == NULL || value->type != RBH_VT_INT64)
        return snprintf(output, max_length, "None");
    else if (value->int64 == INT64_MAX)
        return snprintf(output, max_length, "Inf");
    else
        return snprintf(output, max_length, "%ld", value->int64);
}

static int
write_expires_from_entry(const struct rbh_fsentry *fsentry,
                                 char *output, int max_length)
{
    const struct rbh_value *value =
        rbh_fsentry_find_inode_xattr(fsentry, "user.ccc_expires");

    if (value == NULL || value->type != RBH_VT_STRING)
        return snprintf(output, max_length, "None");
    else
        return snprintf(output, max_length, "%s", value->string);
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
    default:
        return fsentry_print_directive(output, max_length, fsentry, directive,
                                       backend);
    }

    __builtin_unreachable();
}
