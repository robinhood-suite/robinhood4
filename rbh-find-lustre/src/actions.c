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

int
fsentry_print_lustre_directive(char *output, int max_length,
                               const struct rbh_fsentry *fsentry,
                               const char *directive,
                               const char *backend)
{
    assert(directive != NULL);
    assert(*directive != '\0');

    return fsentry_print_directive(output, max_length, fsentry, directive,
                                   backend);
}
