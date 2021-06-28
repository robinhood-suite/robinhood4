/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <stdlib.h>

#include "serialization.h"

/*----------------------------------------------------------------------------*
 |                                  fsevent                                   |
 *----------------------------------------------------------------------------*/

bool
emit_fsevent(yaml_emitter_t *emitter __attribute((unused)),
             const struct rbh_fsevent *fsevent __attribute((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

bool
parse_fsevent(yaml_parser_t *parser __attribute__((unused)),
              struct rbh_fsevent *fsevent __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}
