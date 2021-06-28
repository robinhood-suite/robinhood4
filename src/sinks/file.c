/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>
#include <stdlib.h>

#include "sink.h"

struct sink *
sink_from_file(FILE *file __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}
